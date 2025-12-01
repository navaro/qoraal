// qfs_port.h
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef QFS_PATH_MAX
#define QFS_PATH_MAX 256
#endif

// Directory listing
typedef struct qfs_dir qfs_dir_t;
typedef struct {
    char name[256];
    int  is_dir;  // 1=dir, 0=file, -1=unknown
} qfs_dirent_t;

int  qfs_dir_open(qfs_dir_t **out, const char *path);
int  qfs_dir_read(qfs_dir_t *d, qfs_dirent_t *e); // 0=end, >0=ok, <0=err
void qfs_dir_close(qfs_dir_t *d);

// Files
// Reads whole file into malloc'd buffer; returns size or <0 on error.
int  qfs_read_all(const char *path, char **out_buf);

// PWD/CD abstraction (Zephyr doesn’t have process cwd: we keep a static cwd)
int  qfs_chdir(const char *path);
const char *qfs_getcwd(void);

// Utility: turn relative into absolute using qfs_getcwd()
int  qfs_make_abs(char *out, size_t out_sz, const char *path_in);

// Delete a file (and optionally empty directories if your backend supports it).
int qfs_unlink(const char *path);     // 0 on success, <0 on error
int qfs_rmdir (const char *path);     // 0 on success, <0 on error (optional; for empty dirs)

// Simple portable wildcard match supporting '*' and '?'.
// Returns: 1 = match, 0 = no match.
int qfs_match(const char *pattern, const char *name);


int qfs_mkdir(const char *path) ;

#ifdef __cplusplus
}
#endif
