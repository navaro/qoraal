# Zephyr module integration for Qoraal

zephyr_library()

# Includes
zephyr_include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

# Core sources (adjust if needed)
zephyr_library_sources_ifdef(CONFIG_QORAAL
    ${CMAKE_CURRENT_LIST_DIR}/../src/qoraal.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/debug.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/os_mlock.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/os_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/os_zephyr.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/os.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/qfs_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/qfs_zephyr.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_events.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_logger.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_services.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_shell.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_tasks.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_threads.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/svc/svc_wdt.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/cbuffer.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/dictionary.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/lists.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/memdbg.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/mlog.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/rtclib.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/common/strsub.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/qshell/console.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/qshell/fscmds.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/qshell/servicescmd.c
)

# Optional compile defs (keep minimal for now)
zephyr_compile_definitions(
    CFG_OS_ZEPHYR=1
)

# Translate Zephyr config → Qoraal config
zephyr_compile_definitions_ifndef(CONFIG_QORAAL_QFS CFG_QFS_DISABLE=1)

zephyr_linker_sources(SECTIONS ${CMAKE_CURRENT_LIST_DIR}/rom.ld)
zephyr_linker_sources(DATA_SECTIONS ${CMAKE_CURRENT_LIST_DIR}/ram.ld)