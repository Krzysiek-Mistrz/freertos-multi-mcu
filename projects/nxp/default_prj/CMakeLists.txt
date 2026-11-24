# NXP i.MX 8M Mini EVK, Cortex-M4 core. Built with the MCUXpresso SDK (west), see top level README.
cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

# board files (pin_mux, hardware_init.c -> BOARD_InitHardware, app.h) are borrowed from the SDK freertos_hello example
project(freertos_pipeline_nxp LANGUAGES C CXX ASM
        PROJECT_BOARD_PORT_PATH ${board_root}/${board}/freertos_examples/freertos_hello)

include(${SdkRootDirPath}/CMakeLists.txt)

get_filename_component(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}                  ABSOLUTE)
get_filename_component(COMMON   ${CMAKE_CURRENT_LIST_DIR}/../common        ABSOLUTE)
get_filename_component(EMBEDKIT ${CMAKE_CURRENT_LIST_DIR}/../../embedkit/lib ABSOLUTE)

# mcux_add_source prepends BASE_PATH to every file -> one call per directory
mcux_add_source(BASE_PATH ${APP_ROOT} SOURCES main.c bsp_nxp.c)
mcux_add_source(BASE_PATH ${COMMON}   SOURCES app_pipeline.c app_primitives.c)
mcux_add_source(BASE_PATH ${EMBEDKIT} SOURCES fsm.c)

mcux_add_include(BASE_PATH ${APP_ROOT} INCLUDES .)
mcux_add_include(BASE_PATH ${COMMON}   INCLUDES .)
mcux_add_include(BASE_PATH ${EMBEDKIT} INCLUDES .)

# resource table (remoteproc), needed so Linux/U-Boot can start the M4
include(${SdkRootDirPath}/${board_root}/${board}/freertos_examples/freertos_hello/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
