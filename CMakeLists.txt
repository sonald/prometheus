cmake_minimum_required(VERSION 2.8.11)

set(target prometheusd)
set(client prometheus)

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

if (NOT EXISTS /usr/include/glm)
    message(SEND_ERROR "-- missing glm headers, plz install glm first")
endif()

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
configure_file(${PROJECT_SOURCE_DIR}/systemd/prometheus-quit.service.in 
    ${PROJECT_BINARY_DIR}/systemd/prometheus-quit.service)

set(SRCS atlas.cc driver.cc glutil.cc options.cc scene.cc)

# make all shaders built into executable
file(GLOB SHADERS RELATIVE ${PROJECT_SOURCE_DIR} "*.glsl")
foreach(shader_src ${SHADERS})
    set(shader "${shader_src}.o")
    set(SHADER_OBJS "${SHADER_OBJS}" ${shader})
    message("generate shader target ${shader}")

    add_custom_target(${shader})
    add_custom_command(TARGET ${shader} PRE_BUILD
        COMMAND ld -r -o ${shader} -z noexecstack --format=binary ${shader_src}
        COMMAND objcopy --rename-section .data=.rodata,alloc,load,readonly,data,contents 
        ${shader}
        COMMAND mv ${shader} ${PROJECT_BINARY_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_source_files_properties(
        ${shader} PROPERTIES EXTERNAL_OBJECT TRUE  GENERATED TRUE)
endforeach()

add_executable(${target} ${SRCS} ${SHADER_OBJS})
add_dependencies(${target} ${SHADER_OBJS})

add_executable(${client} client.cc)

target_link_libraries(${target} ${GLESV2_LIBRARIES} ${EGL_LIBRARIES}
    ${GBM_LIBRARIES} ${LIBDRM_LIBRARIES} ${FT2_LIBRARIES})

# install stage
install(TARGETS ${target} ${client} RUNTIME DESTINATION sbin)
install(FILES initrd/prometheus.initrd_hook 
    DESTINATION lib/initcpio/hooks RENAME prometheus)
install(FILES initrd/prometheus.initrd_install
    DESTINATION lib/initcpio/install RENAME prometheus)
# quit service
install(FILES ${PROJECT_BINARY_DIR}/systemd/prometheus-quit.service
    DESTINATION /usr/lib/systemd/system 
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
