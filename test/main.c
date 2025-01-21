#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qoraal/qoraal.h"
#include "qoraal/svc/svc_events.h"
#include "qoraal/svc/svc_tasks.h"
#include "qoraal/svc/svc_logger.h"
#include "qoraal/svc/svc_threads.h"
#include "qoraal/svc/svc_wdt.h"
#include "qoraal/svc/svc_services.h"
#include "qoraal/svc/svc_shell.h"
#include "qoraal/common/mlog.h"
#include "services/services.h"
#include "platform/platform.h"

/*===========================================================================*/
/* Macros and Defines                                                        */
/*===========================================================================*/

/*===========================================================================*/
/* Service Local Variables and Types                                         */
/*===========================================================================*/

QORAAL_SERVC_LIST_START(_qoraal_services_list)
QORAAL_SERVC_RUN_DECL("system",  system_service_run, system_service_ctrl, 0, 6000, OS_THREAD_PRIO_7, QORAAL_SERVICE_SYSTEM, SVC_SERVICE_FLAGS_AUTOSTART)
QORAAL_SERVC_RUN_DECL("shell",  shell_service_run, shell_service_ctrl, 0, 6000, OS_THREAD_PRIO_7, QORAAL_SERVICE_SHELL, SVC_SERVICE_FLAGS_AUTOSTART)
QORAAL_SERVC_DECL("demo", demo_service_ctrl, 0, QORAAL_SERVICE_DEMO, 0)
QORAAL_SERVC_LIST_END()

static const QORAAL_CFG_T       _qoraal_cfg = { .malloc = malloc, .free = free, .debug_print = platform_print, .debug_assert = platform_assert, .current_time = platform_current_time, .wdt_kick = platform_wdt_kick};

/*===========================================================================*/
/* Local Functions                                                           */
/*===========================================================================*/

/**
 * @brief Initializatoin after scheduler was started.
 * @note  If the services completely define the application, you can  
 *        safely exit here, and the service threads will clean up this 
 *        thread's resources.
 * 
 */
static void
main_thread(void* arg)
{
    platform_start () ;
    qoraal_svc_start () ;
}

/**
 * @brief Pre-scheduler initialization.
 *
 */
void
main_init (void)
{
    static SVC_THREADS_T thd ;

    platform_init () ;
    qoraal_instance_init (&_qoraal_cfg) ;
    qoraal_svc_init (_qoraal_services_list) ;

    svc_threads_create (&thd, 0,
                4000, OS_THREAD_PRIO_1, main_thread, 0, 0) ;

}

#if defined CFG_OS_THREADX 
void tx_application_define(void *first_unused_memory)
{
    main_init () ;
}
#endif

/**
 * @brief Main entry point.
 *
 */
int main( void )
{
#if !defined CFG_OS_THREADX
    main_init() ;
#endif

    /* Start the scheduler. */
    os_sys_start ();

    /* 
     * Dependinmg on the RTOS, if you get here it will be in a threading context. 
     * So alternatively, main_thread can be called from here.
     */

    /*
     * For the demo, we wait for the shell to be exited with the "exit" command.
     */
    platform_wait_for_exit (QORAAL_SERVICE_SHELL) ;    

    // for( ;; ) os_thread_sleep (32768);
}



