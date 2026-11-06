// #define CFG_OS_ZEPHYR    1
/* CFG_DEBUG_SVC_LOGGER_DISABLE
    If defined, the logger service will not use svc_logger to route debug logging.
*/
// #define CFG_DEBUG_SVC_LOGGER_DISABLE    1

/* CFG_SVC_THREADS_DISABLE_IDLE
    If defined, system should call svc_threads_complete_check() from an idle thread to cleanup 
    terminated threds.
*/
#if defined CFG_OS_FREERTOS
#define CFG_SVC_THREADS_DISABLE_IDLE    1
#endif

/* CFG_SVC_THREADS_IDLE_PRIO
    Priority of the svc_threads_idle thread, which frees the stacks of
    terminated svc_threads. At the lowest priority it only runs when nothing
    else is ready, so under steady load the stacks of finished threads (web
    server workers, for example) pile up in the heap. Give it the priority of
    the service that creates the threads to free them as they finish.
*/
#if !(defined CFG_SVC_THREADS_IDLE_PRIO)
#define CFG_SVC_THREADS_IDLE_PRIO       OS_THREAD_PRIO_LOWEST
#endif

/* CFG_SVC_SERVICES_START_STACK
    Stack of the thread that starts a service again when a start was asked
    while it stopped (a restart that could not wait for the stop). It runs
    the service's START, which normally runs in the caller's thread, such as
    the shell's: a web server that loads its TLS certificate needs about
    4.6 KB of it.
*/
#if !(defined CFG_SVC_SERVICES_START_STACK)
#define CFG_SVC_SERVICES_START_STACK    8192
#endif


/* CFG_SVC_WDT_DISABLE_PLATFORM
    If defined, platform does not support a watchdog, functions will be implemented as stubs only.
*/
// #define CFG_SVC_WDT_DISABLE_PLATFORM    1


/* CFG_COMMON_STRSUB_DISABLE
    If defined the shell will not support string substitution in the command parser
*/
// #define CFG_COMMON_STRSUB_DISABLE        1


/* CFG_COMMON_MEMLOG_DISABLE 
    If defined, the platform does not support memory logging.
*/
// #define CFG_COMMON_MEMLOG_DISABLE      1


/* CFG_OS_OS_TIMER_DISABLE
    If defined, the platform does not support os timers
*/
// #define CFG_OS_OS_TIMER_DISABLE     1

/* CFG_OS_EVENT_DISABLE
    If defined, the platform does not support os events 
*/
// #define CFG_OS_EVENT_DISABLE        1

/* CFG_OS_MLOCK_DISABLE
    If defined, the platform does not support mlock
*/
// #define CFG_OS_MLOCK_DISABLE        1

/* CFG_QFS_DISABLE
    If defined, the platform does not support qfs
*/
// #define CFG_QFS_DISABLE        1

/* CFG_OS_MEM_DEBUG_ENABLE
    If defined, the platform does aditioanl checking an memory allocations
*/
// #define CFG_OS_MEM_DEBUG_ENABLE        1

/* CFG_OS_MALLOC_DEBUG_ENABLE
    If defined, the platform prepends heap information to all debug messages.
*/
#define CFG_OS_MALLOC_DEBUG_ENABLE        1


/* CFG_QSHELL_CONSOLE_ENABLE
    If defined, a console is included in the build
*/
#define CFG_QSHELL_CONSOLE_ENABLE       1

/* CFG_QSHELL_CONSOLE_ENABLE
    If defined, a filesystem shell is included in the build
*/
#if !(defined CFG_QFS_DISABLE) || !CFG_QFS_DISABLE
#define CFG_QSHELL_FS_ENABLE            1
#endif

/* CFG_QSHELL_SERVICES_ENABLE
    If defined, a services shell is included in the build
*/
#define CFG_QSHELL_SERVICES_ENABLE       1

#if !(defined CFG_SVC_TASK_CFG_MAX)
#define CFG_SVC_TASK_CFG_MAX             3
#endif
#if !(defined CFG_SVC_TASK_CFG_DEFAULT)
#define CFG_SVC_TASK_CFG_DEFAULT    {OS_THREAD_PRIO_10, 1024*2, "svc-task0", TIMEOUT_10_SEC}    \
                                    ,{OS_THREAD_PRIO_6, 1024*6, "svc-task1", TIMEOUT_30_SEC}    \
                                    ,{OS_THREAD_PRIO_2, 1024*4, "svc-task2", TIMEOUT_10_SEC}
#endif

#if !(defined CFG_SVC_EVENTS_THREAD_PRIO)
#define CFG_SVC_EVENTS_THREAD_PRIO       OS_THREAD_PRIO_11
#endif

#if !(defined CFG_CONSOLE_PROMPT)
#define CFG_CONSOLE_PROMPT              "[Qoraal] #> "
#endif

/* CFG_CONSOLE_HISTORY_DEPTH
    Number of commands the console keeps for recall with the up and down
    arrows. Each one costs CFG_CONSOLE_HISTORY_LEN + 1 bytes of RAM. 0 leaves
    the history out.
*/
#if !(defined CFG_CONSOLE_HISTORY_DEPTH)
#define CFG_CONSOLE_HISTORY_DEPTH       8
#endif

/* CFG_CONSOLE_HISTORY_LEN
    Longest command, in characters, the console history stores. A longer
    command is not stored at all rather than stored cut off, so recalling it
    can never run something else than was typed. The shell line itself stays
    255 characters.
*/
#if !(defined CFG_CONSOLE_HISTORY_LEN)
#define CFG_CONSOLE_HISTORY_LEN         128
#endif
