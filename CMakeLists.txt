cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project ("WinterRenderer")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(Vulkan)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/vk-bootstrap)
add_subdirectory(external/VulkanMemoryAllocator)

add_executable(WinterRenderer
    "src/Main.cpp"  
    "src/renderer.h" 
    "src/renderer.cpp"
)

set(IMGUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp"
)
target_sources(WinterRenderer PRIVATE ${IMGUI_SOURCES})

target_include_directories(WinterRenderer PRIVATE 
    src
    external/glm
    external/stb
    external/imgui
    external/imgui/backends
)

target_link_libraries(WinterRenderer PRIVATE
    glfw
    Vulkan::Vulkan
    vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
)