
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
#if CFG_QSHELL_CONSOLE_ENABLE
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "qoraal/qoraal.h"
#include "qoraal/svc/svc_services.h"
#include "qoraal/svc/svc_shell.h"
#include "qoraal/qshell/console.h"


/*===========================================================================*/
/* Macros and Defines                                                        */
/*===========================================================================*/

SVC_SERVICES_T      _console_service_id = SVC_SERVICES_INVALID ;   
bool                _console_echo_on = 
#if defined CFG_OS_POSIX 
    false ;
#else
    true ; 
#endif

#define DBG_MESSAGE_SHELL(severity, fmt_str, ...)   DBG_MESSAGE_T_REPORT (SVC_LOGGER_TYPE(severity,0), _console_service_id, fmt_str, ##__VA_ARGS__)

#define SHELL_VERSION_STR   "Navaro Qoraal Demo v '" __DATE__ "'"
#define SHELL_HELLO         "Enter 'help' or '?' to view available commands. "
#define SHELL_PROMPT        CFG_CONSOLE_PROMPT

/*===========================================================================*/
/* Service Local Functions                                                   */
/*===========================================================================*/
static void     console_logger_cb (void* channel, LOGGER_TYPE_T type, uint8_t facility, const char* msg) ;
static int32_t  console_out (void* ctx, uint32_t out, const char* str);
static int32_t  console_print (const char * str) ;
static int32_t  console_get_line (char * buffer, uint32_t len) ;



SVC_SHELL_CMD_DECL("exit", qshell_cmd_exit, "");
SVC_SHELL_CMD_DECL("version", qshell_cmd_version, "");
SVC_SHELL_CMD_DECL("hello", qshell_cmd_hello, "");
SVC_SHELL_CMD_DECL("loglevel", qshell_loglevel, "[level]");



int qshell_uart_init(void) ;

/*===========================================================================*/
/* Service Local Variables and Types                                         */
/*===========================================================================*/

static bool                 _shell_exit = false ;
static LOGGER_CHANNEL_T     _shell_log_channel = { .fp = console_logger_cb, .user = (void*)0, .filter = { { .mask = SVC_LOGGER_MASK, .type = SVC_LOGGER_SEVERITY_LOG | SVC_LOGGER_FLAGS_PROGRESS }, {0,0} } };

/* ---- command history (last 8 commands, browsable with up/down arrows) ---- */
#define CONSOLE_HISTORY_DEPTH   8
#define CONSOLE_HISTORY_LEN     256
#define CONSOLE_PUSHBACK_EMPTY  (-2)

static char     _history[CONSOLE_HISTORY_DEPTH][CONSOLE_HISTORY_LEN] ;
static uint32_t _history_count = 0 ;   /* number of valid entries (<= DEPTH) */
static uint32_t _history_head = 0 ;    /* slot where the NEXT entry goes */
static char     _history_scratch[CONSOLE_HISTORY_LEN] ; /* uncommitted line saved while browsing */
static int32_t  _history_nav = -1 ;    /* -1 = not browsing, else 0 = newest ... */
static int32_t  _history_pushback = CONSOLE_PUSHBACK_EMPTY ; /* one-char pushback for key sequences */

/* Read one char, honouring the single-char pushback buffer. */
static int
console_history_getch (void)
{
    if (_history_pushback != CONSOLE_PUSHBACK_EMPTY) {
        int c = _history_pushback ;
        _history_pushback = CONSOLE_PUSHBACK_EMPTY ;
        return c ;
    }
    return qoraal_getch (1000) ;
}

/* Store a finished line in the history ring (skips empty lines). */
static void
console_history_add (const char * line)
{
    char tmp[CONSOLE_HISTORY_LEN] ;
    size_t n ;
    if (!line || !line[0]) return ;
    strncpy (tmp, line, sizeof (tmp) - 1) ;
    tmp[sizeof (tmp) - 1] = 0 ;
    n = strlen (tmp) ;
    while (n > 0 && (tmp[n - 1] == '\n' || tmp[n - 1] == '\r')) tmp[--n] = 0 ;
    if (tmp[0] == 0) return ;
    strncpy (_history[_history_head], tmp, CONSOLE_HISTORY_LEN - 1) ;
    _history[_history_head][CONSOLE_HISTORY_LEN - 1] = 0 ;
    _history_head = (_history_head + 1) % CONSOLE_HISTORY_DEPTH ;
    if (_history_count < CONSOLE_HISTORY_DEPTH) _history_count++ ;
}

/* Fetch a history entry by age: 0 = newest. Returns 0 when out of range. */
static const char *
console_history_get (int32_t age)
{
    uint32_t idx ;
    if (age < 0 || (uint32_t)age >= _history_count) return 0 ;
    idx = (_history_head + CONSOLE_HISTORY_DEPTH - 1 - (uint32_t)age) % CONSOLE_HISTORY_DEPTH ;
    return _history[idx] ;
}

/* Redraw the input line in a terminal-agnostic way: CR, blank the old
 * content (plus `extra` bytes the platform may already have echoed for the
 * key sequence), CR, prompt, new text.
 * NOTE: everything is emitted as ONE console_print() call. console_print()
 * routes each call through the debug-log prefix ("[%5d] "), so per-char
 * prints would spray the prefix once per character. */
static void
console_history_redraw (const char * text, uint32_t old_len)
{
    /* prompt (16) + worst-case old line (256) + echo slack + text (256) + CRs */
    static char redraw[576] ;
    uint32_t clear_n, text_n, n, pos = 0 ;
    clear_n = (uint32_t)strlen (SHELL_PROMPT) + old_len + 2 ;
    if (clear_n > 300) clear_n = 300 ;
    text_n = text ? (uint32_t)strlen (text) : 0 ;
    if (text_n > CONSOLE_HISTORY_LEN - 1) text_n = CONSOLE_HISTORY_LEN - 1 ;
    redraw[pos++] = '\r' ;
    for (n = 0 ; n < clear_n && pos + 1 < sizeof (redraw) ; n++) redraw[pos++] = ' ' ;
    redraw[pos++] = '\r' ;
    n = (uint32_t)strlen (SHELL_PROMPT) ;
    memcpy (&redraw[pos], SHELL_PROMPT, n) ; pos += n ;
    if (text) { memcpy (&redraw[pos], text, text_n) ; pos += text_n ; }
    redraw[pos] = 0 ;
    console_print (redraw) ;
}

/* Browse history: dir > 0 recalls older (up), dir < 0 recalls newer (down).
 * Updates the edit buffer (*pi) and redraws the line. `extra` = number of
 * bytes the platform may already have echoed for the key sequence. */
static void
console_history_browse (int dir, char * buffer, uint32_t len, uint32_t * pi, uint32_t extra)
{
    const char * entry = 0 ;
    uint32_t old_len = *pi + extra ;
    uint32_t n = 0 ;
    if (_history_count == 0) return ;
    if (dir > 0) {                          /* up: older command */
        if (_history_nav < 0) {
            strncpy (_history_scratch, buffer, CONSOLE_HISTORY_LEN - 1) ;
            _history_scratch[CONSOLE_HISTORY_LEN - 1] = 0 ;
            _history_nav = 0 ;
        } else if (_history_nav + 1 < (int32_t)_history_count) {
            _history_nav++ ;
        }
        entry = console_history_get (_history_nav) ;
    } else {                                /* down: newer command */
        if (_history_nav > 0) {
            _history_nav-- ;
            entry = console_history_get (_history_nav) ;
        } else if (_history_nav == 0) {
            _history_nav = -1 ;
            entry = _history_scratch ;      /* restore the uncommitted line */
        } else {
            return ;                        /* not browsing: ignore */
        }
    }
    if (!entry) entry = "" ;
    while (entry[n] && n + 1 < len) {
        buffer[n] = entry[n] ;
        n++ ;
    }
    *pi = n ;
    buffer[n] = 0 ;
    console_history_redraw (buffer, old_len) ;
}


/*===========================================================================*/
/* Service Functions                                                         */
/*===========================================================================*/

/**
 * @brief       console_service_ctrl
 * @details
 * @note        For code SVC_SERVICE_CTRL_STATUS, if the return value is E_NOIMPL
 *              the status will be determined by the svc_services module.
 *
 * @param[in] code
 * @param[in] arg
 *
 * @return      status
 *
 * @services
 */
int32_t
console_service_ctrl (uint32_t code, uintptr_t arg)
{
    int32_t res = EOK ;

    switch (code) {
    case SVC_SERVICE_CTRL_INIT:
#if !defined(CFG_QFS_DISABLE) || !CFG_QFS_DISABLE
        extern void svc_shell_fscmds_force_link (void) ;
        svc_shell_fscmds_force_link () ;
#endif
        _console_service_id = svc_service_service ((SCV_SERVICE_HANDLE) arg ) ;

        // the implementation on the platform should check if the 
        // uart is ready before writing to it, so we can safely 
        // add the log channel here.
        svc_logger_channel_add (&_shell_log_channel) ;

        break ;

    case SVC_SERVICE_CTRL_START:
        break ;

    case SVC_SERVICE_CTRL_STOP: 
        DBG_MESSAGE_SHELL(DBG_MESSAGE_SEVERITY_LOG, "SHELL : : shell shutting down...");
        svc_logger_channel_remove (&_shell_log_channel) ;
        _shell_exit = true;
        break ;

    case SVC_SERVICE_CTRL_STATUS:
    default:
        res = E_NOIMPL ;
        break ;

    }

    return res ;
}

int32_t
console_print (const char* str)
{
        qoraal_debug_print (str) ;

    return  SVC_SHELL_CMD_E_OK ;
}


/**
 * @brief       console_service_run
 * @details     Runs the shell service, processing input and executing commands
 *              until the "exit" command is issued.
 *
 * @param[in]   arg     Argument passed to the service.
 *
 * @return      status  The result of the shell execution.
 */
int32_t
console_service_run (uintptr_t arg)
{
    DBG_MESSAGE_SHELL (DBG_MESSAGE_SEVERITY_INFO, "SHELL : : shell STARTED");

    SVC_SHELL_IF_T  qshell_cmd_if ;
    svc_shell_if_init (&qshell_cmd_if, 0, console_out, 0) ;


    /*
     * Now process the input from the command line as shell commands until
     * the "exit" command is executed.
     */
    svc_shell_script_run (&qshell_cmd_if, "", "version", strlen("version")) ;
    svc_shell_script_run (&qshell_cmd_if, "", "hello", strlen("hello")) ;
    do {
        char line[256];
        console_print (SHELL_PROMPT) ;
        int len = console_get_line (line, sizeof(line)) ;
        if (!_shell_exit && len > 0) {
            svc_shell_script_run (&qshell_cmd_if, "", line, len) ;
            
        }

    } while (!_shell_exit) ;


    return EOK ;
}

/**
 * @brief       console_out
 * @details     Handles shell output operations.
 *
 * @param[in]   ctx     The context for the output operation.
 * @param[in]   out     The output channel.
 * @param[in]   str     The string to output.
 *
 * @return      status  The result of the operation.
 */
int32_t
console_out (void* ctx, uint32_t out, const char* str)
{
    if (str && (out && out < SVC_SHELL_IN_STD)) {
        if (out == SVC_SHELL_OUT_SYS) qoraal_debug_print (str) ;
        else qoraal_print (str) ;

    }

    return  SVC_SHELL_CMD_E_OK ;
}

/**
 * @brief       console_get_line
 * @details     Reads a line of input from the user.
 *
 * @param[out]  buffer  The buffer to store the input line.
 * @param[in]   len     The maximum length of the buffer.
 *
 * @return      length  The length of the input line.
 */
int32_t console_get_line(char *buffer, uint32_t len)
{
    static bool swallow_lf = false;
    uint32_t i = 0;

    if (!buffer || len == 0) return 0;
    buffer[0] = 0;

    _history_nav = -1 ; /* fresh line: not browsing history */

    while (i + 1 < len) {
        int c = console_history_getch ();

        if (_shell_exit) break;

        if (c <= 0) {
            if (c == EOF) os_thread_sleep(1000);
            continue;
        }

        /* Escape sequences: up/down arrows arrive as ESC [ A / ESC [ B. */
        if (c == 27) {
            int c2 = console_history_getch () ;
            if (c2 != '[') {
                _history_pushback = c2 ;    /* not a CSI sequence: replay it */
                continue ;
            }
            c2 = console_history_getch () ;
            if (c2 == 'A') console_history_browse (+1, buffer, len, &i, 2) ;
            else if (c2 == 'B') console_history_browse (-1, buffer, len, &i, 2) ;
            else _history_pushback = c2 ;   /* unknown CSI: replay final byte */
            continue ;
        }
#if defined _WIN32
        /* Windows console: arrows arrive as 0x00/0xE0 + scan code (72/80). */
        if (c == 0 || c == 224) {
            int c2 = console_history_getch () ;
            if (c2 == 72) console_history_browse (+1, buffer, len, &i, 1) ;
            else if (c2 == 80) console_history_browse (-1, buffer, len, &i, 1) ;
            else _history_pushback = c2 ;
            continue ;
        }
#endif

        /* swallow LF after CR across calls */
        if (swallow_lf) {
            swallow_lf = false;
            if (c == '\n') continue;
        }

        /* backspace (BS or DEL) */
        if (c == '\b' || c == 0x7f) {
            if (i > 0) {
                i--;
                buffer[i] = 0;
                if (_console_echo_on) qoraal_print("\b \b");   // erase visually
            }
            _history_nav = -1 ; /* edited: leave history browsing */
            continue;
        }

        /* end-of-line handling */
        if (c == '\r') {
            swallow_lf = true;
            buffer[i++] = '\n';
            buffer[i] = 0;
            if (_console_echo_on) qoraal_print("\r\n");
            break;
        }

        buffer[i++] = (char)c;
        buffer[i] = 0;

        /* echo */
        if (c == '\n') {
            if (_console_echo_on) qoraal_print("\r\n");
            break;
        } else {
            if (_console_echo_on) qoraal_print(&buffer[i-1]);
        }
        _history_nav = -1 ; /* typed: leave history browsing */
    }

    buffer[i] = 0;
    if (!_shell_exit) console_history_add (buffer) ;
    return (int32_t)i;
}

/**
 * @brief       console_logger_cb
 * @details     Callback function for logging messages from the shell.
 *
 * @param[in]   channel     The logger channel.
 * @param[in]   type        The type of log message.
 * @param[in]   facility    The logging facility.
 * @param[in]   msg         The log message to display.
 */
void
console_logger_cb (void* channel, LOGGER_TYPE_T type, uint8_t facility, const char* msg)
{
    if ((SVC_LOGGER_GET_FLAGS(type) & SVC_LOGGER_FLAGS_NO_FORMATTING)) {
        qoraal_print(msg) ;
    }  else {
        qoraal_debug_print(msg) ;
        qoraal_debug_print ("\r\n") ;
    }
}

typedef struct {
    SVC_SERVICES_T id ;
    p_sem_t sem ;
} CONSOLE_EXIT_T ;

static void 
status_callback (SVC_SERVICES_T  id, int32_t status, uintptr_t parm)
{
    CONSOLE_EXIT_T * pexit = (CONSOLE_EXIT_T *)parm ;
    if ((status == SVC_SERVICE_STATUS_STOPPED || status == SVC_SERVICE_STATUS_STOPPING) && 
        (id == pexit->id || id == _console_service_id)) {
        os_sem_signal (&pexit->sem) ;
    }
}

void
console_wait_for_exit (SVC_SERVICES_T  id)
{
    CONSOLE_EXIT_T exit ;
    p_sem_t    stop_sem ;
    os_sem_create (&stop_sem, 0) ;

    exit.sem = stop_sem ;
    exit.id = id ;

    SVC_SERVICE_HANDLER_T  handler ;
    svc_service_register_handler (&handler, status_callback, (uintptr_t) &exit) ;
    os_sem_wait (&stop_sem) ;
    svc_service_unregister_handler (&handler) ;
    os_sem_delete (&stop_sem) ;
}


/**
 * @brief       qshell_cmd_version
 * @details     Outputs the version of the Qoraal shell.
 *
 * @param[in]   pif     Shell interface pointer.
 * @param[in]   argv    Command-line arguments.
 * @param[in]   argc    Number of command-line arguments.
 *
 * @return      status  The result of the command execution.
 */
int32_t
qshell_cmd_version (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    svc_shell_print (pif, SVC_SHELL_OUT_STD, "%s\r\n", SHELL_VERSION_STR) ;
    return SVC_SHELL_CMD_E_OK ;
}

/**
 * @brief       qshell_cmd_hello
 * @details     Outputs hello text of the Qoraal shell.
 *
 * @param[in]   pif     Shell interface pointer.
 * @param[in]   argv    Command-line arguments.
 * @param[in]   argc    Number of command-line arguments.
 *
 * @return      status  The result of the command execution.
 */
int32_t
qshell_cmd_hello (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    svc_shell_print (pif, SVC_SHELL_OUT_STD, "%s\r\n\r\n", SHELL_HELLO) ;


    return SVC_SHELL_CMD_E_OK ;
}

/**
 * @brief       qshell_cmd_exit
 * @details     Exits the shell service.
 *
 * @param[in]   pif     Shell interface pointer.
 * @param[in]   argv    Command-line arguments.
 * @param[in]   argc    Number of command-line arguments.
 *
 * @return      status  The result of the command execution.
 */
int32_t
qshell_cmd_exit (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    _shell_exit = true ;
    return SVC_SHELL_CMD_E_OK ;
}


/**
 * @brief       qshell_cmd_exit
 * @details     Exits the shell service.
 *
 * @param[in]   pif     Shell interface pointer.
 * @param[in]   argv    Command-line arguments.
 * @param[in]   argc    Number of command-line arguments.
 *
 * @return      status  The result of the command execution.
 */
int32_t
qshell_loglevel (SVC_SHELL_IF_T * pif, char** argv, int argc)
{
    uint32_t val = SVC_LOGGER_GET_SEVERITY(_shell_log_channel.filter[0].type) ;
    if (argc > 1) {

        if (svc_shell_scan_int (argv[1], (uint32_t*)&val) == EOK) {
            SVC_LOGGER_SET_SEVERITY(_shell_log_channel.filter[0].type, val) ;

            svc_logger_channel_remove (&_shell_log_channel) ;
            svc_logger_channel_add (&_shell_log_channel) ;
        }
    }

    svc_shell_print (pif, SVC_SHELL_OUT_STD, "Filter level %d\r\n", val) ;
    return SVC_SHELL_CMD_E_OK ;
}


#endif
