#include "qoraal/config.h"
#if defined CFG_OS_POSIX
#include "qoraal/qfs_port.h"
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

int qfs_dir_open(qfs_dir_t **out, const char *path) {
    DIR *dp = opendir(path ? path : ".");
    if (!dp) return -1;
    *out = malloc(sizeof(**out));
    if (!*out) {
        closedir(dp);
        return -1;
    }
    (*out)->dp = dp;
    return 1;
}

int qfs_dir_read(qfs_dir_t *d, qfs_dirent_t *e) {
    struct dirent *de = readdir(d->dp);
    if (!de) return 0;

    strncpy(e->name, de->d_name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = 0;

    /* Not all platforms provide d_type / DT_DIR (e.g. some MinGW dirent.h). */
#ifdef DT_DIR
    e->is_dir = (de->d_type == DT_DIR) ? 1 : 0;
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
        free(d);
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

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return -5;
    }

    size_t n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    if (n != (size_t)sz) {
        free(buf);
        return -6;
    }

    buf[sz] = 0;
    *out_buf = buf;
    return (int)sz;
}

static char cwd_buf[256];

int qfs_chdir(const char *path) {
    return chdir(path);
}

const char *qfs_getcwd(void) {
    return getcwd(cwd_buf, sizeof(cwd_buf));
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
    if (!cwd) return -1;

#ifdef _WIN32
    int n = snprintf(out, out_sz, "%s\\%s", cwd, in);
#else
    int n = snprintf(out, out_sz, "%s/%s", cwd, in);
#endif
    return (n < 0 || (size_t)n >= out_sz) ? -1 : 0;
}

int qfs_unlink(const char *path) {
    return (remove(path) == 0) ? 0 : -errno;
}

int qfs_rmdir(const char *path) {
    return (rmdir(path) == 0) ? 0 : -errno;
}

// Simple fallback wildcard matcher for Windows (supports * and ?)
static int simple_match(const char *pattern, const char *name) {
    const char *p = pattern;
    const char *n = name;
    const char *star = NULL;
    const char *retry = NULL;

    while (*n) {
        if (*p == '*') {
            star = p++;
            retry = n;
        } else if (*p == '?' || *p == *n) {
            p++;
            n++;
        } else if (star) {
            p = star + 1;
            n = ++retry;
        } else {
            return 0;
        }
    }

    while (*p == '*') p++;
    return (*p == '\0');
}

int qfs_match(const char *pattern, const char *name) {
#ifdef _WIN32
    return simple_match(pattern, name);
#else
    return (fnmatch(pattern, name, 0) == 0);
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
#endif /* CFG_OS_ZEPHYR */
