cmake_minimum_required(VERSION 2.8.11)

set(target prometheus)
project(${target})

SET( CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(prometheus_VERSION_MAJOR 0)
set(prometheus_VERSION_MINOR 1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLESV2 REQUIRED glesv2)
pkg_check_modules(LIBDRM REQUIRED libdrm)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GBM REQUIRED gbm)
pkg_check_modules(FT2 REQUIRED freetype2)

#TODO: check for glm headers

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${GLESV2_INCLUDE_DIRS})
include_directories(${EGL_INCLUDE_DIRS})
include_directories(${GBM_INCLUDE_DIRS})
include_directories(${LIBDRM_INCLUDE_DIRS})
include_directories(${GLFW3_INCLUDE_DIRS})
include_directories(${FT2_INCLUDE_DIRS})


configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)

file(GLOB SRCS "${PROJECT_SOURCE_DIR}/*.cc")

# make atlas shaders builtin
set(SHADERS atlas_frag.glsl.o atlas_vertex.glsl.o)
foreach(shader ${SHADERS})
add_custom_target(${shader})
string(REPLACE ".o" "" shader_src ${shader})
add_custom_command(TARGET ${shader}
    PRE_BUILD
    COMMAND ld -r -o ${shader} -z noexecstack --format=binary ${shader_src}
    COMMAND objcopy --rename-section .data=.rodata,alloc,load,readonly,data,contents 
    ${shader}
    COMMAND mv ${shader} ${PROJECT_BINARY_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endforeach()

add_executable(${target} ${SRCS} ${SHADERS})
add_dependencies(${target} ${SHADERS})

target_link_libraries(${target} ${GLESV2_LIBRARIES} ${EGL_LIBRARIES}
    ${GBM_LIBRARIES} ${LIBDRM_LIBRARIES} ${FT2_LIBRARIES})
