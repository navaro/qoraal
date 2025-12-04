#include "qoraal/config.h"
#if defined CFG_OS_ZEPHYR
#include "qoraal/qfs.h"
#include "qoraal/qoraal.h"
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define QFS_ROOT "/lfs"

struct qfs_dir { struct fs_dir_t dir; int opened; };

struct qfs_file { struct fs_file_t file; int opened; };

static char s_cwd[256] = QFS_ROOT; // emulate a cwd rooted at /lfs

/* Put near the top, after includes and QFS_ROOT. */
static int normalize_path(char *path, size_t path_sz)
{
    /* Require prefix /lfs; clamp .. to that root. */
    const size_t root_len = strlen(QFS_ROOT);
    if (strncmp(path, QFS_ROOT, root_len) != 0) return -2;

    char tmp[QFS_PATH_MAX];
    size_t out = 0;

    /* start with /lfs */
    if (root_len >= sizeof(tmp)) return -3;
    memcpy(tmp, QFS_ROOT, root_len);
    out = root_len;

    /* walk the rest, splitting on '/' */
    const char *s = path + root_len;
    while (*s) {
        /* skip repeated slashes */
        while (*s == '/') s++;

        /* component start */
        const char *seg = s;
        while (*s && *s != '/') s++;
        size_t seglen = (size_t)(s - seg);
        if (seglen == 0) break;

        if (seglen == 1 && seg[0] == '.') {
            /* ignore "." */
            continue;
        }

        if (seglen == 2 && seg[0] == '.' && seg[1] == '.') {
            /* back up one level, but never above /lfs */
            if (out > root_len) {
                /* remove last component */
                while (out > root_len && tmp[out - 1] != '/') {
                    out--;
                }
                if (out > root_len && tmp[out - 1] == '/') out--;
            }
            continue;
        }

        /* add '/' if needed */
        if (out + 1 >= sizeof(tmp)) return -4;
        if (out == 0 || tmp[out - 1] != '/') {
            tmp[out++] = '/';
        }

        /* append segment */
        if (out + seglen >= sizeof(tmp)) return -4;
        memcpy(&tmp[out], seg, seglen);
        out += seglen;
    }

    /* remove trailing slash (but keep "/lfs") */
    if (out > root_len && tmp[out - 1] == '/') out--;

    tmp[out] = '\0';

    /* write back */
    if (out + 1 > path_sz) return -5;
    memcpy(path, tmp, out + 1);
    return 0;
}

static int make_path(char *out, size_t out_sz, const char *in)
{
    /* Interpret `in` relative to our cwd buffer, rooted at /lfs. */
    if (!in || !in[0]) {
        strncpy(out, s_cwd, out_sz);
        out[out_sz - 1] = 0;
        return 0;
    }

    if (in[0] == '/') {
        /* absolute path wrt /lfs root */
        strncpy(out, in, out_sz);
        out[out_sz - 1] = 0;
    } else {
        /* relative path: cwd + "/" + in */
        size_t cwd_len = strlen(s_cwd);
        size_t need = cwd_len + 1 + strlen(in) + 1;
        if (need > out_sz) return -1;

        memcpy(out, s_cwd, cwd_len);
        if (cwd_len == 0 || out[cwd_len - 1] != '/') {
            out[cwd_len++] = '/';
        }
        strcpy(&out[cwd_len], in);
    }

    /* normalize it, clamp ".." components */
    return normalize_path(out, out_sz);
}

int qfs_dir_open(qfs_dir_t **out, const char *path)
{
    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    char p[QFS_PATH_MAX];
    int rc = make_path(p, sizeof(p), path ? path : s_cwd);
    if (rc) return rc;

    rc = fs_opendir(&dir, p);
    if (rc) return rc;

    qfs_dir_t *h = malloc(sizeof(*h));
    if (!h) {
        fs_closedir(&dir);
        return E_NOMEM;
    }

    h->dir = dir;
    h->opened = 1;
    *out = h;
    return 0;
}

int qfs_dir_read(qfs_dir_t *d, qfs_dirent_t *e)
{
    if (!d || !e) return -EINVAL;

    struct fs_dirent ent;
    int rc = fs_readdir(&d->dir, &ent);
    if (rc) return -rc;

    if (ent.name[0] == 0) return 0; // end
    strncpy(e->name, ent.name, sizeof(e->name)-1);
    e->name[sizeof(e->name)-1] = 0;
    e->is_dir = (ent.type == FS_DIR_ENTRY_DIR) ? 1 :
                (ent.type == FS_DIR_ENTRY_FILE) ? 0 : -1;
    return 1;
}

void qfs_dir_close(qfs_dir_t *d) {
    if (!d) return;
    if (d->opened) fs_closedir(&d->dir);
    free(d);
}

int qfs_read_all(const char *path, char **out_buf) {
    *out_buf = NULL;

    char p[QFS_PATH_MAX];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;

    struct fs_file_t f;
    fs_file_t_init(&f);

    rc = fs_open(&f, p, FS_O_READ);
    if (rc) return rc;

    struct fs_dirent st;
    rc = fs_stat(p, &st);
    if (rc || st.size <= 0) {
        fs_close(&f);
        return rc ? rc : -EIO;
    }

    char *buf = qoraal_malloc(QORAAL_HeapAuxiliary, st.size + 1);
    if (!buf) {
        fs_close(&f);
        return -ENOMEM;
    }

    ssize_t n = fs_read(&f, buf, st.size);
    fs_close(&f);

    if (n != (ssize_t)st.size) {
        qoraal_free(QORAAL_HeapAuxiliary, buf);
        return -EIO;
    }

    buf[st.size] = '\0';
    *out_buf = buf;
    return (int)st.size;
}

int qfs_open(qfs_file_t **out, const char *path, int flags)
{
    (void)flags;
    if (!out || !path) return -EINVAL;
    *out = NULL;

    char p[QFS_PATH_MAX];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;

    qfs_file_t *h = malloc(sizeof(*h));
    if (!h) return E_NOMEM;

    fs_file_t_init(&h->file);
    rc = fs_open(&h->file, p, FS_O_CREATE | FS_O_TRUNC | FS_O_WRITE);
    if (rc) {
        free(h);
        return rc;
    }

    h->opened = 1;
    *out = h;
    return 0;
}

int qfs_write(qfs_file_t *f, const void *buf, size_t len)
{
    if (!f || !f->opened || !buf) return -EINVAL;
    ssize_t w = fs_write(&f->file, buf, len);
    if (w < 0) {
        return (int)w;
    }
    return (int)w;
}

int qfs_close(qfs_file_t *f)
{
    if (!f) return 0;
    int rc = 0;

    if (f->opened) {
        rc = fs_close(&f->file);
        f->opened = 0;
    }

    free(f);
    return rc;
}

int qfs_chdir(const char *path)
{
    char p[QFS_PATH_MAX];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;

    /* verify it's a directory */
    struct fs_dirent st;
    rc = fs_stat(p, &st);
    if (rc) return rc;
    if (st.type != FS_DIR_ENTRY_DIR) return E_NOTFOUND;

    strncpy(s_cwd, p, sizeof(s_cwd) - 1);
    s_cwd[sizeof(s_cwd) - 1] = 0;
    return 0;
}

const char *qfs_getcwd(void) { return s_cwd; }

int qfs_make_abs(char *out, size_t out_sz, const char *in) {
    return make_path(out, out_sz, in);
}

// Reuse the internal make_path(...) from earlier implementation
// static int make_path(char *out, size_t out_sz, const char *in);

int qfs_unlink(const char *path) {
    char p[QFS_PATH_MAX];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;
    rc = fs_unlink(p);              // files only
    return rc ? -rc : 0;
}

int qfs_rmdir(const char *path) {
    char p[QFS_PATH_MAX];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;
    rc = fs_unlink(p);              // we treat rmdir as unlink for now
    return rc ? -rc : 0;
}

// Minimal '*' and '?' wildcard matcher (no char classes, ranges, etc.)
static int match_simple(const char *pat, const char *str) {
    const char *ps = NULL, *ss = NULL;

    while (*str) {
        if (*pat == '*') { ps = ++pat; ss = str; continue; }
        if (*pat == '?' || *pat == *str) { pat++; str++; continue; }
        if (ps) { pat = ps; str = ++ss; continue; }
        return 0;
    }
    while (*pat == '*') pat++;
    return *pat == '\0';
}

int qfs_match(const char *pattern, const char *name) {
    return match_simple(pattern, name) ? 1 : 0;
}

#endif /*CFG_OS_ZEPHYR*/
