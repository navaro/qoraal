// qfs_port_zephyr.c
#include "qfs_port.h"
#include "qoraal/qoraal.h"
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define QFS_ROOT "/lfs"

struct qfs_dir { struct fs_dir_t dir; int opened; };

static char s_cwd[256] = QFS_ROOT; // emulate a cwd rooted at /lfs

static int make_path(char *out, size_t out_sz, const char *in) {
    if (!in || !in[0]) return -1;
    if (in[0] == '/') {
        // absolute: must already start with /lfs
        if (strncmp(in, QFS_ROOT, strlen(QFS_ROOT)) != 0) return -2;
        strncpy(out, in, out_sz); out[out_sz-1]=0; return 0;
    }
    // relative: join with cwd
    int n = snprintf(out, out_sz, "%s/%s", s_cwd, in);
    return (n < 0 || (size_t)n >= out_sz) ? -3 : 0;
}

int qfs_dir_open(qfs_dir_t **out, const char *path) {
    *out = NULL; char p[256];
    int rc = make_path(p, sizeof(p), path ? path : ".");
    if (rc) return rc;
    qfs_dir_t *h = calloc(1, sizeof(*h));
    if (!h) return E_NOMEM;
    fs_dir_t_init(&h->dir);
    rc = fs_opendir(&h->dir, p);
    if (rc) { free(h); return rc; }
    h->opened = 1; *out = h; return 1;
}

int qfs_dir_read(qfs_dir_t *d, qfs_dirent_t *e) {
    struct fs_dirent ent; int rc = fs_readdir(&d->dir, &ent);
    if (rc) return -rc;         // error
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
    *out_buf = NULL; char p[256];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;

    struct fs_file_t f; fs_file_t_init(&f);
    rc = fs_open(&f, p, FS_O_READ);
    if (rc) return rc;

    struct fs_dirent st;
    rc = fs_stat(p, &st);
    if (rc || st.size <= 0) { fs_close(&f); return rc ? rc : EFAIL; }

    char *buf = malloc(st.size + 1);
    if (!buf) { fs_close(&f); return E_NOMEM; }

    ssize_t n = fs_read(&f, buf, st.size);
    fs_close(&f);
    if (n != (ssize_t)st.size) { free(buf); return EFAIL; }

    buf[st.size] = 0; *out_buf = buf; return (int)st.size;
}

int qfs_chdir(const char *path) {
    char p[256];
    int rc = make_path(p, sizeof(p), path);
    if (rc) return rc;
    // verify it's a dir
    struct fs_dirent st;
    rc = fs_stat(p, &st);
    if (rc) return rc;
    if (st.type != FS_DIR_ENTRY_DIR) return E_NOTFOUND;
    strncpy(s_cwd, p, sizeof(s_cwd)-1);
    s_cwd[sizeof(s_cwd)-1] = 0;
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
    rc = fs_rmdir(p);               // empty directories
    return rc ? -rc : 0;
}

// Minimal '*' and '?' wildcard matcher (no char classes, no escapes).
static int match_simple(const char *pat, const char *str) {
    // Classic backtracking glob
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
