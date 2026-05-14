cmake_minimum_required(VERSION 3.15)
project(hid_proxy LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "This project targets Windows only.")
endif()

# Must be built as a shared library (DLL)
add_library(hid SHARED
    src/cs/Main.cpp
    src/cs/core/Bootstrap.cpp
    src/cs/core/MinHookStubs.cpp
    src/cs/features/FreezeGranny.cpp
    src/cs/hooks/Hooks.cpp
    src/cs/ui/Menu.cpp
    src/cs/util/Logger.cpp
)

target_include_directories(hid PRIVATE src/cs)

# Use the MSVC runtime statically so the proxy has no extra runtime dependency
set_property(TARGET hid PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_compile_features(hid PRIVATE cxx_std_17)

# Output the DLL as "hid.dll"
set_target_properties(hid PROPERTIES
    OUTPUT_NAME "hid"
    PREFIX      ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

set(MINHOOK_LIB "")
set(MINHOOK_CANDIDATES
    "${CMAKE_SOURCE_DIR}/libMinHook-x64.lib"
    "${CMAKE_SOURCE_DIR}/libMinHook.x64.lib"
    "${CMAKE_SOURCE_DIR}/MinHook.x64.lib"
    "${CMAKE_SOURCE_DIR}/libMinHook.lib"
)

foreach(candidate IN LISTS MINHOOK_CANDIDATES)
    if (EXISTS "${candidate}")
        set(MINHOOK_LIB "${candidate}")
        break()
    endif()
endforeach()

if (NOT MINHOOK_LIB STREQUAL "")
    message(STATUS "Using MinHook library: ${MINHOOK_LIB}")
    target_compile_definitions(hid PRIVATE HAVE_MINHOOK=1)
    target_link_libraries(hid PRIVATE "${MINHOOK_LIB}")
else()
    message(WARNING "MinHook library not found in project root. Hook installation will be disabled at runtime.")
endif()

add_executable(injector
    src/injector/Injector.cpp
)

set_property(TARGET injector PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_features(injector PRIVATE cxx_std_17)
set_target_properties(injector PROPERTIES
    OUTPUT_NAME "inject"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist")

add_custom_target(package_dist ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:hid>" "${DIST_DIR}/hid.dll"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:injector>" "${DIST_DIR}/inject.exe"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/src/resources/mappings.json" "${DIST_DIR}/resources/mappings.json"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/src/resources/flagged_methods.json" "${DIST_DIR}/resources/flagged_methods.json"
    DEPENDS hid injector
    COMMENT "Packaging binaries and resources into dist"
)
