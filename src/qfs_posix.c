#include "qoraal/config.h"
#if defined CFG_OS_POSIX  && (!defined(CFG_QFS_DISABLE) || !CFG_QFS_DISABLE)

#include "qoraal/qoraal.h"
#include "qoraal/qfs.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <fnmatch.h>
#endif

struct qfs_dir { DIR *dp; };

struct qfs_file { FILE *fp; };

int qfs_dir_open(qfs_dir_t **out, const char *path) {
    DIR *dp = opendir(path ? path : ".");
    if (!dp) return -1;
    *out = qoraal_malloc(QORAAL_HeapAuxiliary, sizeof(**out));
    if (!*out) {
        closedir(dp);
        return -1;
    }
    (*out)->dp = dp;
    return 0;
}

int qfs_dir_read(qfs_dir_t *d, qfs_dirent_t *e) {
    if (!d || !e) return -1;
    struct dirent *de = readdir(d->dp);
    if (!de) return 0; // end

    strncpy(e->name, de->d_name, sizeof(e->name)-1);
    e->name[sizeof(e->name)-1] = 0;

#if defined(_DIRENT_HAVE_D_TYPE)
    if (de->d_type == DT_DIR)      e->is_dir = 1;
    else if (de->d_type == DT_REG) e->is_dir = 0;
    else                           e->is_dir = -1;
#else
    /* Fallback: we don’t know, so report “not a dir”. Callers should
       tolerate this on platforms without d_type. */
    e->is_dir = 0;
#endif

    return 1;
}

void qfs_dir_close(qfs_dir_t *d) {
    if (d) {
        closedir(d->dp);
        qoraal_free(QORAAL_HeapAuxiliary, d);
    }
}

int qfs_read_all(const char *path, char **out_buf) {
    *out_buf = NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -2;
    }

    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return -3;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -4;
    }

    char *buf = qoraal_malloc(QORAAL_HeapAuxiliary, (size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return -5;
    }

    size_t n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    if (n != (size_t)sz) {
        qoraal_free(QORAAL_HeapAuxiliary, buf);
        return -6;
    }

    buf[sz] = 0;
    *out_buf = buf;
    return (int)sz;
}

static char cwd_buf[256];

int qfs_open(qfs_file_t **out, const char *path, int flags)
{
    (void)flags;
    if (!out || !path) return -EINVAL;
    *out = NULL;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return -errno;
    }

    qfs_file_t *h = qoraal_malloc(QORAAL_HeapAuxiliary, sizeof(*h));
    if (!h) {
        fclose(fp);
        return -ENOMEM;
    }

    h->fp = fp;
    *out  = h;
    return 0;
}

int qfs_write(qfs_file_t *f, const void *buf, size_t len)
{
    if (!f || !f->fp || !buf) return -EINVAL;

    size_t n = fwrite(buf, 1, len, f->fp);
    if (n == len) {
        return (int)n;
    }

    if (ferror(f->fp)) {
        return -EIO;
    }

    /* Short write without ferror() set – unusual, but report bytes written. */
    return (int)n;
}

int qfs_close(qfs_file_t *f)
{
    if (!f) return 0;

    int rc = 0;
    if (f->fp) {
        rc = fclose(f->fp);
        if (rc != 0) {
            rc = -errno;
        }
        f->fp = NULL;
    }

    qoraal_free(QORAAL_HeapAuxiliary, f);
    return rc;
}

int qfs_chdir(const char *path) {
    return chdir(path);
}

const char *qfs_getcwd(void) {
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) {
        cwd_buf[0] = 0;
    }
    return cwd_buf;
}

int qfs_make_abs(char *out, size_t out_sz, const char *in) {
    if (!in || !in[0]) return -1;

#ifdef _WIN32
    /* Very simple check for an absolute path on Windows (drive letter or UNC). */
    if ((strlen(in) > 2 && in[1] == ':' && (in[2] == '\\' || in[2] == '/')) ||
        (in[0] == '\\' && in[1] == '\\')) {
        strncpy(out, in, out_sz);
        out[out_sz - 1] = 0;
        return 0;
    }
#else
    if (in[0] == '/') {
        strncpy(out, in, out_sz);
        out[out_sz - 1] = 0;
        return 0;
    }
#endif

    const char *cwd = qfs_getcwd();
    size_t cwd_len = strlen(cwd);
    size_t in_len  = strlen(in);

    if (cwd_len + 1 + in_len + 1 > out_sz) return -1;
    memcpy(out, cwd, cwd_len);
#ifdef _WIN32
    out[cwd_len++] = '\\';
#else
    out[cwd_len++] = '/';
#endif
    memcpy(out + cwd_len, in, in_len + 1);
    return 0;
}

int qfs_unlink(const char *path) {
    int rc = unlink(path);
    if (rc == 0) return 0;
    if (errno == ENOENT) return 0;
    return -errno;
}

int qfs_rmdir(const char *path) {
    int rc = rmdir(path);
    if (rc == 0) return 0;
    if (errno == ENOENT) return 0;
    return -errno;
}

int qfs_match(const char *pattern, const char *name) {
#if defined(_WIN32)
    /* fnmatch isn’t standard on Windows; you can stub or implement a
       simple matcher here if needed. For now: basic strcmp. */
    (void)pattern;
    (void)name;
    // TODO: implement wildcard match on Windows if needed.
    return strcmp(pattern, name) == 0;
#else
    int rc = fnmatch(pattern, name, 0);
    return (rc == 0) ? 1 : 0;
#endif
}

int qfs_mkdir(const char *path)
{
    /* On POSIX: mkdir(path, mode); on Windows: mkdir(path); */
#if defined(_WIN32)
    int rc = mkdir(path);
#else
    int rc = mkdir(path, 0777);
#endif
    if (rc < 0 && errno == EEXIST) return 0;  /* ok if it already exists */
    return rc;
}
#endif /* CFG_OS_POSIX */
