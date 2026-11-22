# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(stm32)

# Enable CMake support for ASM and C/CPP languages
enable_language(C CXX ASM)

set(PROJECT_NAME stm32)

# Create an executable object type
add_executable(${PROJECT_NAME})

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Third party code lives in git submodules at the repo root (see top level README):
#   third_party/FreeRTOS-Kernel   https://github.com/FreeRTOS/FreeRTOS-Kernel
#   third_party/CMSIS_6           https://github.com/ARM-software/CMSIS_6
#   third_party/cmsis_device_f4   https://github.com/STMicroelectronics/cmsis_device_f4
# Override with -D<VAR>=<path> to use checkouts from elsewhere.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(FREERTOS_KERNEL_DIR "${REPO_ROOT}/third_party/FreeRTOS-Kernel" CACHE PATH "FreeRTOS-Kernel checkout")
set(CMSIS_CORE_DIR      "${REPO_ROOT}/third_party/CMSIS_6/CMSIS/Core" CACHE PATH "CMSIS core (contains Include/core_cm4.h)")
set(CMSIS_DEVICE_DIR    "${REPO_ROOT}/third_party/cmsis_device_f4" CACHE PATH "cmsis_device_f4 (Include/, Source/Templates/)")

# Add sources to executable/library
target_sources(${PROJECT_NAME} PRIVATE
  # board layer
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/bsp_stm32.c"
  # shared application and my library
  "${REPO_ROOT}/projects/common/app_pipeline.c"
  "${REPO_ROOT}/projects/common/app_primitives.c"
  "${REPO_ROOT}/embedkit/lib/fsm.c"
  # FreeRTOS kernel, Cortex-M4F port, heap_4
  "${FREERTOS_KERNEL_DIR}/tasks.c"
  "${FREERTOS_KERNEL_DIR}/queue.c"
  "${FREERTOS_KERNEL_DIR}/list.c"
  "${FREERTOS_KERNEL_DIR}/timers.c"
  "${FREERTOS_KERNEL_DIR}/event_groups.c"
  "${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F/port.c"
  "${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c"
  # SystemInit (called from the startup file)
  "${CMSIS_DEVICE_DIR}/Source/Templates/system_stm32f4xx.c"
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Inc"
  "${REPO_ROOT}/projects/common"
  "${REPO_ROOT}/embedkit/lib"
  "${FREERTOS_KERNEL_DIR}/include"
  "${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F"
  "${CMSIS_CORE_DIR}/Include"
  "${CMSIS_DEVICE_DIR}/Include"
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
  # User defined compiler options
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32F411xE        # STM32F411xC and xE share the same device header
  HSE_VALUE=25000000 # Black Pill crystal
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE 
  # User defined linker options
)

# Add additional linker search directories
target_link_directories(${PROJECT_NAME} PRIVATE
  # Add additional linker search directories here
)

# Add additional linker libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  # Add additional linker libraries here
)
