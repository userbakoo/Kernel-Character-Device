cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)
project(driver VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find kernel headers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(KernelHeaders REQUIRED)
include_directories(${KERNELHEADERS_INCLUDE_DIRS})
message(STATUS "Kernel directories: ${KERNELHEADERS_INCLUDE_DIRS}")
add_definitions(-D__KERNEL__ -DMODULE)
set(OUTPUT_FILE my_module.ko)
set(SOURCES my_module.c)
set(KBUILD_COMMAND $(MAKE) -C ${KERNELHEADERS_DIR} modules M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR})
FILE(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/Kbuild "obj-m := my_module.o")
add_custom_command(OUTPUT ${OUTPUT_FILE}
        COMMAND ${KBUILD_COMMAND}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${SOURCES} VERBATIM)

add_custom_target(module ALL DEPENDS ${OUTPUT_FILE})

# -- Only for development purposes --
add_executable(dmy_tgt ${SOURCES})
target_include_directories(dmy_tgt PRIVATE ${KERNELHEADERS_INCLUDE_DIRS})
# -------


