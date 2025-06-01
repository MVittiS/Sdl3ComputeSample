cmake_minimum_required(VERSION 3.20)
project(Sdl3ComputeSample)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

find_package(SDL3 REQUIRED)
find_package(spdlog CONFIG REQUIRED)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Werror)
endif()

if (MSVC)
    set(DXC_PATH dxc)
else ()
    if (NOT DXC_PATH)
        message(FATAL_ERROR "Please set 'DXC_PATH' when configuring.")
    endif()
#     set(DXC_PATH $ENV{DXC_PATH})
endif()
    
add_custom_command(
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/cs.hlsl
    OUTPUT
        ${CMAKE_BINARY_DIR}/cs.dxil
        # ${CMAKE_BINARY_DIR}/cs.depfile
        # ${CMAKE_BINARY_DIR}/cs.rootsig.json
    COMMAND
        ${DXC_PATH}
        -T cs_6_0
        -E main
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/cs.hlsl
        -Fo ${CMAKE_BINARY_DIR}/cs.dxil
        # -Frs ${CMAKE_BINARY_DIR}/cs.rootsig.json
        -Zi
        -Qembed_debug
        -O0
    #     -MF ${CMAKE_BINARY_DIR}/cs.depfile
    # DEPFILE
    #     ${CMAKE_BINARY_DIR}/cs.depfile
    VERBATIM
)

if (APPLE)
    add_custom_command(
        DEPENDS
            ${CMAKE_BINARY_DIR}/cs.dxil
        OUTPUT
            ${CMAKE_BINARY_DIR}/cs.mtlib
            ${CMAKE_BINARY_DIR}/cs.reflect.json
        COMMAND
            metal-shaderconverter
                ${CMAKE_BINARY_DIR}/cs.dxil
                --deployment-os=macOS
                -o=${CMAKE_BINARY_DIR}/cs.mtlib
                --output-reflection-file=${CMAKE_BINARY_DIR}/cs.reflect.json
        VERBATIM
    )

    add_custom_target(Shaders
        DEPENDS
            ${CMAKE_BINARY_DIR}/cs.mtlib
    )
elseif(WIN32)
    add_custom_target(Shaders
        DEPENDS
            ${CMAKE_BINARY_DIR}/cs.dxil
    )
endif()

add_executable(ComputeHeadless
    src/Main.cpp
)
target_compile_features(ComputeHeadless
    PUBLIC
        cxx_std_17
)
target_include_directories(ComputeHeadless
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(ComputeHeadless
    PRIVATE
        -fsanitize=address
)
target_link_libraries(ComputeHeadless
    PRIVATE
        SDL3::SDL3
        spdlog::spdlog
)
target_link_options(ComputeHeadless
    PRIVATE
        -fsanitize=address
)
add_dependencies(ComputeHeadless
    Shaders
)

if (MSVC)
    target_compile_definitions(ComputeHeadless
        PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
endif()
