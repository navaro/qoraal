
/*
    Copyright (C) 2015-2025, Navaro, All Rights Reserved
    SPDX-License-Identifier: MIT

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
 */

#include "qoraal/config.h"
#if CFG_QSHELL_FS_ENABLE
#include <stdio.h>
#include "qoraal/qoraal.h"
#include "qoraal/svc/svc_events.h"
#include "qoraal/svc/svc_tasks.h"
#include "qoraal/svc/svc_logger.h"
#include "qoraal/svc/svc_threads.h"
#include "qoraal/svc/svc_wdt.h"
#include "qoraal/svc/svc_services.h"
#include "qoraal/svc/svc_shell.h"

/*
    Shell commands (portable FS version)
    Uses qfs.h to target either POSIX or Zephyr (LittleFS at /lfs).

    Copyright (C) 2015-2025, Navaro
    SPDX-License-Identifier: MIT
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "qoraal/qfs.h"
#include "qoraal/svc/svc_shell.h"

/* -------------------------------------------------------------------------- */
/*  ls                                                                        */
/* -------------------------------------------------------------------------- */

static int32_t
qshell_cmd_ls (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    /* List directory contents; default to "." if no argument is given. */
    const char *dir = (argc > 1) ? argv[1] : ".";

    qfs_dir_t *d = NULL;
    int rc = qfs_dir_open(&d, dir);
    if (rc <= 0 || d == NULL) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "Unable to read directory\r\n");
        return SVC_SHELL_CMD_E_FAIL;
    }

    qfs_dirent_t e;
    while ((rc = qfs_dir_read(d, &e)) > 0) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "%s\r\n", e.name);
    }

    qfs_dir_close(d);
    return SVC_SHELL_CMD_E_OK;
}

/* -------------------------------------------------------------------------- */
/*  cd                                                                        */
/* -------------------------------------------------------------------------- */

static int32_t
qshell_cmd_cd (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    if (argc < 2) {
        return SVC_SHELL_CMD_E_PARMS;
    }

    if (qfs_chdir(argv[1]) != 0) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "failed\r\n");
        return SVC_SHELL_CMD_E_FAIL;
    }

    return SVC_SHELL_CMD_E_OK;
}

/* -------------------------------------------------------------------------- */
/*  pwd                                                                       */
/* -------------------------------------------------------------------------- */

static int32_t
qshell_cmd_pwd (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    (void)argv;
    (void)argc;

    const char *cwd = qfs_getcwd();
    if (!cwd || !cwd[0]) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "unable to get current directory.\r\n");
        return SVC_SHELL_CMD_E_FAIL;
    }

    svc_shell_print(pif, SVC_SHELL_OUT_STD, "%s\r\n", cwd);
    return SVC_SHELL_CMD_E_OK;
}

/* -------------------------------------------------------------------------- */
/*  helper: read file via qfs                                                 */
/* -------------------------------------------------------------------------- */

static int32_t
read_file_qfs (SVC_SHELL_IF_T * pif, const char * filename, char ** pbuffer)
{
    *pbuffer = NULL;

    int n = qfs_read_all(filename, pbuffer);
    if (n < 0 || *pbuffer == NULL) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "unable to open/read file \"%s\".\r\n", filename);
        return (n < 0) ? SVC_SHELL_CMD_E_FAIL : SVC_SHELL_CMD_E_NOT_FOUND;
    }

    return n; /* size in bytes */
}

/* -------------------------------------------------------------------------- */
/*  source (and alias ".")                                                    */
/* -------------------------------------------------------------------------- */

static int32_t
qshell_cmd_source (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    int32_t res;

    if (argc < 2) {
        return SVC_SHELL_CMD_E_PARMS;
    }

    char *buffer;
    res = read_file_qfs(pif, argv[1], &buffer);
    if (res > 0) {
        /* Run the script read from the file. */
        int32_t run_res = svc_shell_script_run(pif, "", buffer, res);
        qoraal_free(QORAAL_HeapAuxiliary, buffer);
        return run_res;
    }

    return res;
}

/* -------------------------------------------------------------------------- */
/*  cat                                                                       */
/* -------------------------------------------------------------------------- */

static int32_t
qshell_cmd_cat (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    if (argc < 2) {
        return SVC_SHELL_CMD_E_OK;
    }

    char *buffer;
    int32_t res = read_file_qfs(pif, argv[1], &buffer);
    if (res > 0) {
        /* Print file as-is. */
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "%s", buffer);
        qoraal_free(QORAAL_HeapAuxiliary, buffer);
        return SVC_SHELL_CMD_E_OK;
    }

    return res;
}

/* -------------------------------------------------------------------------- */
/*  echo                                                                      */
/* -------------------------------------------------------------------------- */

static int32_t
qshell_cmd_echo (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    /*
     * Echo the first argument. Demonstrates string substitution
     * for registry strings, e.g., "echo [test]"
     */
    if (argc < 2) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "\r\n");
    } else {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "%s\r\n", argv[1]);
    }

    return SVC_SHELL_CMD_E_OK;
}

// shell_cmds.c (append alongside your other commands)
#ifndef ALLOW_RMDIR
#define ALLOW_RMDIR 0
#endif

static int32_t
qshell_cmd_rm (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    if (argc < 2) {
        return SVC_SHELL_CMD_E_PARMS;
    }

    const char *arg = argv[1];

    // Quick check for wildcard presence
    const int has_wild = (strchr(arg, '*') != NULL) || (strchr(arg, '?') != NULL);

    if (!has_wild) {
        // Single path delete
        char abs[QFS_PATH_MAX];
        if (qfs_make_abs(abs, sizeof(abs), arg) != 0) {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "error: bad path\r\n");
            return SVC_SHELL_CMD_E_FAIL;
        }

        // Try unlink first. If it is a directory and unlink fails with EISDIR
        // you can optionally try rmdir when ALLOW_RMDIR == 1.
        int rc = qfs_unlink(abs);
#if ALLOW_RMDIR
        if (rc < 0) {
            // attempt rmdir for empty directories
            if (qfs_rmdir(abs) == 0) {
                svc_shell_print(pif, SVC_SHELL_OUT_STD, "deleted dir: %s\r\n", abs);
                return SVC_SHELL_CMD_E_OK;
            }
        }
#endif
        if (rc < 0) {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "error %d deleting %s\r\n", rc, abs);
            return SVC_SHELL_CMD_E_FAIL;
        }

        svc_shell_print(pif, SVC_SHELL_OUT_STD, "deleted: %s\r\n", abs);
        return SVC_SHELL_CMD_E_OK;
    }

    // --- Wildcard path: split directory and pattern
    char path[QFS_PATH_MAX];
    if (qfs_make_abs(path, sizeof(path), arg) != 0) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "error: bad path\r\n");
        return SVC_SHELL_CMD_E_FAIL;
    }

    char dir_path[QFS_PATH_MAX];
    const char *pattern = NULL;

    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        size_t len = (size_t)(last_slash - path);
        if (len >= sizeof(dir_path)) len = sizeof(dir_path) - 1;
        memcpy(dir_path, path, len);
        dir_path[len] = '\0';
        pattern = last_slash + 1;
    } else {
        // relative pattern in current directory
        const char *cwd = qfs_getcwd();
        if (!cwd) cwd = ".";
        strncpy(dir_path, cwd, sizeof(dir_path)-1);
        dir_path[sizeof(dir_path)-1] = '\0';
        pattern = path; // the whole thing is the pattern
    }

    // Open directory
    qfs_dir_t *dir = NULL;
    if (qfs_dir_open(&dir, dir_path) <= 0 || !dir) {
        svc_shell_print(pif, SVC_SHELL_OUT_STD, "error opening directory\r\n");
        return SVC_SHELL_CMD_E_FAIL;
    }

    int32_t overall_rc = SVC_SHELL_CMD_E_OK;
    qfs_dirent_t ent;
    while (qfs_dir_read(dir, &ent) > 0) {
        if (ent.name[0] == '\0') continue;
        if (!qfs_match(pattern, ent.name)) continue;

        // Build full path
        char full[QFS_PATH_MAX];
        int n = snprintf(full, sizeof(full), "%s/%s", dir_path, ent.name);
        if (n <= 0 || n >= (int)sizeof(full)) {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "skip (path too long): %s\r\n", ent.name);
            overall_rc = SVC_SHELL_CMD_E_FAIL;
            continue;
        }

        if (ent.is_dir == 1) {
#if ALLOW_RMDIR
            int rc = qfs_rmdir(full);
            if (rc < 0) {
                svc_shell_print(pif, SVC_SHELL_OUT_STD, "error %d deleting dir %s\r\n", rc, full);
                overall_rc = SVC_SHELL_CMD_E_FAIL;
            } else {
                svc_shell_print(pif, SVC_SHELL_OUT_STD, "deleted dir: %s\r\n", full);
            }
#else
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "skip dir: %s\r\n", full);
#endif
            continue;
        }

        int rc = qfs_unlink(full);
        if (rc < 0) {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "error %d deleting %s\r\n", rc, full);
            overall_rc = SVC_SHELL_CMD_E_FAIL;
        } else {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "deleted: %s\r\n", full);
        }
    }

    qfs_dir_close(dir);
    return overall_rc;
}

/* -------------------------------------------------------------------------- */
/*  mkdir                                                                     */
/* -------------------------------------------------------------------------- */

static int mkdir_parents(const char *path_in)
{
    /* Create all path components, ignoring "already exists" errors. */
    char path[QFS_PATH_MAX];
    size_t len = strnlen(path_in, sizeof(path)-1);
    if (len == 0) return -1;

    /* normalize into a mutable buffer */
    strncpy(path, path_in, sizeof(path)-1);
    path[sizeof(path)-1] = '\0';

    /* If absolute, start after leading '/' group */
    size_t i = 0;
    if (path[0] == '/') i = 1;

    for (; i < strlen(path); ++i) {
        if (path[i] == '/') {
            char saved = path[i];
            path[i] = '\0';
            if (path[0] != '\0') {
                /* Ignore failure for intermediate components (already exists, etc.) */
                (void)qfs_mkdir(path);
            }
            path[i] = saved;
        }
    }

    /* Create the final leaf */
    return qfs_mkdir(path);
}

static int32_t
qshell_cmd_mkdir (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    if (argc < 2) {
        return SVC_SHELL_CMD_E_PARMS; /* usage error */
    }

    int parents = 0;
    int first_path_idx = 1;

    if (strcmp(argv[1], "-p") == 0) {
        parents = 1;
        first_path_idx = 2;
        if (argc < 3) return SVC_SHELL_CMD_E_PARMS;
    }

    int32_t overall = SVC_SHELL_CMD_E_OK;

    for (int i = first_path_idx; i < argc; ++i) {
        const char *arg = argv[i];

        char abs[QFS_PATH_MAX];
        if (qfs_make_abs(abs, sizeof(abs), arg) != 0) {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "error: bad path: %s\r\n", arg);
            overall = SVC_SHELL_CMD_E_FAIL;
            continue;
        }

        int rc = parents ? mkdir_parents(abs) : qfs_mkdir(abs);
        if (rc < 0) {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "mkdir failed (%d): %s\r\n", rc, abs);
            overall = SVC_SHELL_CMD_E_FAIL;
        } else {
            svc_shell_print(pif, SVC_SHELL_OUT_STD, "created: %s\r\n", abs);
        }
    }

    return overall;
}

/* -------------------------------------------------------------------------- */
/*  Command declarations                                                      */
/* -------------------------------------------------------------------------- */

SVC_SHELL_CMD_DECL("ls",     qshell_cmd_ls,     "");
SVC_SHELL_CMD_DECL("cd",     qshell_cmd_cd,     "<path>");
SVC_SHELL_CMD_DECL("source", qshell_cmd_source, "<file>");
SVC_SHELL_CMD_DECL(".",      qshell_cmd_source, "<file>");
SVC_SHELL_CMD_DECL("cat",    qshell_cmd_cat,    "<file>");
SVC_SHELL_CMD_DECL("pwd",    qshell_cmd_pwd,    "");
SVC_SHELL_CMD_DECL("echo",   qshell_cmd_echo,   "[string]");
SVC_SHELL_CMD_DECL("rm",     qshell_cmd_rm,     "[file]");
SVC_SHELL_CMD_DECL("mkdir",  qshell_cmd_mkdir,  "[-p] <path> [more_paths...]");

/* This function exists only to force this module into any final binary
 * that wants shell commands. It does nothing at runtime.
 */
void svc_shell_fscmds_force_link(void)
{
    /* intentionally empty */
}

#endif /* CFG_OS_POSIX */


