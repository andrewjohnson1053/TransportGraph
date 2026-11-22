cmake_minimum_required(VERSION 3.24)
project(TransportGraph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

set(ENGINE_SOURCES
    src/core/App.cpp
    src/core/Camera2D.cpp
    src/core/HitTesting.cpp
    src/sim/Graph.cpp
    src/gfx/VectorRenderer.cpp
    src/ui/InspectorUI.cpp
)

set(ENGINE_HEADERS
    src/core/App.h
    src/core/Camera2D.h
    src/core/HitTesting.h
    src/core/Types.h
    src/sim/Graph.h
    src/sim/SimLoop.h
    src/gfx/VectorRenderer.h
    src/ui/InspectorUI.h
)

add_executable(TransportGraph
    src/main.cpp
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

target_include_directories(TransportGraph PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(TransportGraph PRIVATE
    SDL3::SDL3
    glm::glm
    nlohmann_json::nlohmann_json
    imgui::imgui
)

# Compiler warnings
if(MSVC)
    set(PROJECT_WARNING_OPTIONS /W4 /permissive-)
else()
    set(PROJECT_WARNING_OPTIONS -Wall -Wextra -Wpedantic)
endif()
target_compile_options(TransportGraph PRIVATE ${PROJECT_WARNING_OPTIONS})

# Copy assets next to executable after build
add_custom_command(TARGET TransportGraph POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:TransportGraph>/assets
)

# Verify post-build asset copying
enable_testing()
add_executable(GraphSmokeTest tests/graph_smoke_test.cpp)
target_compile_options(GraphSmokeTest PRIVATE ${PROJECT_WARNING_OPTIONS})
add_test(NAME GraphSmokeTest COMMAND GraphSmokeTest)
set_tests_properties(GraphSmokeTest PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:TransportGraph>"
)

add_executable(CorePrimitivesTest
    tests/core_primitives_test.cpp
    src/core/Camera2D.cpp
    src/core/HitTesting.cpp
)
target_include_directories(CorePrimitivesTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(CorePrimitivesTest PRIVATE glm::glm)
target_compile_options(CorePrimitivesTest PRIVATE ${PROJECT_WARNING_OPTIONS})
add_test(NAME CorePrimitivesTest COMMAND CorePrimitivesTest)

add_executable(VectorRendererTest
    tests/vector_renderer_test.cpp
    src/gfx/VectorRenderer.cpp
    src/core/Camera2D.cpp
)
target_include_directories(VectorRendererTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(VectorRendererTest PRIVATE SDL3::SDL3 glm::glm)
target_compile_options(VectorRendererTest PRIVATE ${PROJECT_WARNING_OPTIONS})
add_test(NAME VectorRendererTest COMMAND VectorRendererTest)
