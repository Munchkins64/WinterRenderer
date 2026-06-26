cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("WinterRenderer")

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED)

add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/vk-bootstrap)
add_subdirectory(external/VulkanMemoryAllocator)

add_executable(WinterRenderer
    "src/Main.cpp"
    "src/renderer.h"
    "src/renderer.cpp"
    
    "src/vma_include.cpp"
)

target_include_directories(WinterRenderer PRIVATE
    src
    external/glm
    external/stb
)

target_link_libraries(WinterRenderer PRIVATE
    glfw
    Vulkan::Vulkan
    vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
)

if(MSVC)
    # Enable multi-core compilation in Visual Studio
    add_compile_options(/MP)
endif()
target_compile_definitions(WinterRenderer PRIVATE USE_CPP20_MODULES)
target_precompile_headers(WinterRenderer PRIVATE <vulkan/vulkan_raii.hpp>)

# Compile GLSL shaders to SPIR-V at build time
find_program(GLSLC glslc HINTS ENV VULKAN_SDK)

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADERS
    "${SHADER_SRC_DIR}/fractal.vert"
    "${SHADER_SRC_DIR}/fractal.frag"
)

foreach(SHADER ${SHADERS})
    get_filename_component(FNAME ${SHADER} NAME)
    set(SPV "${SHADER_OUT_DIR}/${FNAME}.spv")
    add_custom_command(
        OUTPUT  ${SPV}
        COMMAND ${GLSLC} --target-env=vulkan1.3 ${SHADER} -o ${SPV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${FNAME}"
    )
    list(APPEND SPV_FILES ${SPV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_FILES})
add_dependencies(WinterRenderer Shaders)

# Copy compiled shaders next to the executable
add_custom_command(TARGET WinterRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SHADER_OUT_DIR}"
    "$<TARGET_FILE_DIR:WinterRenderer>/shaders"
)