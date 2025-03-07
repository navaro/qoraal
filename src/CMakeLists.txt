# Minimum CMake version (match your project’s requirement)
cmake_minimum_required(VERSION 3.15)

# Collect all source files
set(QORAAL_SRCS
    qoraal.c
    debug.c
    os_mlock.c
    os_posix.c
    os.c
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
    example/console.c
    example/platform.c
    example/posixcmd.c
    qshell/servicescmd.c
)


# Create a static library target named 'qoraal'
add_library(qoraal STATIC ${QORAAL_SRCS})

# Set compile-time definition CFG_OS_FREERTOS for the qoraal target
target_compile_definitions(qoraal PRIVATE CFG_OS_FREERTOS)

# Optional: Set include directories for the library if needed
target_include_directories(qoraal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Add ../include as a PUBLIC include directory
target_include_directories(qoraal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Inherit project-wide includes from sdk_intf_lib
target_link_libraries(qoraal PUBLIC sdk_intf_lib)