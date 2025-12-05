# Collect all source files
set(QORAAL_SRCS
    qoraal.c
    debug.c
    os_mlock.c
    os_posix.c
    os_zephyr.c
    os.c
    qfs_posix.c
    qfs_zephyr.c
    svc/svc_events.c
    svc/svc_logger.c
    svc/svc_services.c
    svc/svc_shell.c
    svc/svc_tasks.c
    svc/svc_threads.c
    svc/svc_wdt.c
    common/cbuffer.c
    common/dictionary.c
    common/lists.c
    common/memdbg.c
    common/mlog.c
    common/rtclib.c
    common/strsub.c
    qshell/console.c
    qshell/fscmds.c
    qshell/servicescmd.c
)

# Add the library target
add_library(qoraal STATIC ${QORAAL_SRCS})

# Set target include dirs
target_include_directories(qoraal PUBLIC ${PROJECT_SOURCE_DIR}/include)

# --- Zephyr integration ---
if (TARGET zephyr_interface)
    message(STATUS "Qoraal: building with Zephyr support")
    target_link_libraries(qoraal PUBLIC zephyr_interface)
    target_compile_definitions(qoraal PUBLIC CFG_OS_ZEPHYR=1)
endif()