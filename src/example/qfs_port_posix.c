// qfs_port_posix.c
#include "qoraal/qfs_port.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct qfs_dir { DIR *dp; };

int qfs_dir_open(qfs_dir_t **out, const char *path) {
    DIR *dp = opendir(path ? path : ".");
    if (!dp) return -1;
    *out = malloc(sizeof(**out));
    (*out)->dp = dp;
    return 1;
}
int qfs_dir_read(qfs_dir_t *d, qfs_dirent_t *e) {
    struct dirent *de = readdir(d->dp);
    if (!de) return 0;
    strncpy(e->name, de->d_name, sizeof(e->name)-1);
    e->name[sizeof(e->name)-1] = 0;
    e->is_dir = (de->d_type == DT_DIR) ? 1 : 0;
    return 1;
}
void qfs_dir_close(qfs_dir_t *d){ if (d){ closedir(d->dp); free(d);} }

int qfs_read_all(const char *path, char **out_buf) {
    *out_buf = NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char *buf = malloc(sz + 1); if (!buf){ fclose(fp); return -2; }
    size_t n = fread(buf, 1, sz, fp); fclose(fp);
    if (n != (size_t)sz){ free(buf); return -3; }
    buf[sz] = 0; *out_buf = buf; return (int)sz;
}

static char cwd_buf[256];
int qfs_chdir(const char *path){ return chdir(path); }
const char *qfs_getcwd(void){ return getcwd(cwd_buf, sizeof(cwd_buf)); }

int qfs_make_abs(char *out, size_t out_sz, const char *in) {
    if (!in || !in[0]) return -1;
    if (in[0] == '/') { strncpy(out, in, out_sz); out[out_sz-1]=0; return 0; }
    const char *cwd = qfs_getcwd(); if (!cwd) return -1;
    int n = snprintf(out, out_sz, "%s/%s", cwd, in);
    return (n < 0 || (size_t)n >= out_sz) ? -1 : 0;
}

int qfs_unlink(const char *path) {
    return (remove(path) == 0) ? 0 : -errno;
}

int qfs_rmdir(const char *path) {
    return (rmdir(path) == 0) ? 0 : -errno;
}

int qfs_match(const char *pattern, const char *name) {
    return (fnmatch(pattern, name, 0) == 0) ? 1 : 0;
}

int qfs_mkdir(const char *path)
{
    int rc = mkdir(path, 0777);
    if (rc < 0 && errno == EEXIST) return 0;  /* ok if it already exists */
    return rc;
}