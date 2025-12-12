cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(qoraal_test)

# target_sources(app PRIVATE src/main.c)
zephyr_link_libraries(qoraal)
set(TEST_SRCS
    ../common/main.c
    ../common/services/demo.c
    ../common/services/system.c
    src/platform.c
)
target_sources(app PRIVATE ${TEST_SRCS})

set(QORAAL_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

add_subdirectory(
    ${QORAAL_ROOT}
    ${CMAKE_CURRENT_BINARY_DIR}/qoraal
)


# Suppose the library defined in the repo is called "qoraal"
target_link_libraries(qoraal      PUBLIC zephyr_interface)
target_compile_definitions(qoraal PRIVATE CFG_OS_ZEPHYR=1 CFG_QFS_DISABLE=1)
target_compile_definitions(app    PRIVATE CFG_OS_ZEPHYR=1)

zephyr_linker_sources(SECTIONS src/zephyr.ld)

target_link_libraries(app PRIVATE qoraal)
