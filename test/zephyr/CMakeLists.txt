cmake_minimum_required(VERSION 3.20.0)

set(QORAAL_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../)

list(APPEND ZEPHYR_EXTRA_MODULES ${QORAAL_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(qoraal_test)

set(TEST_SRCS
    ../common/main.c
    ../common/services/demo.c
    ../common/services/system.c
    src/platform.c
)

target_sources(app PRIVATE ${TEST_SRCS})

target_compile_definitions(app PRIVATE
    CFG_OS_ZEPHYR=1
)

