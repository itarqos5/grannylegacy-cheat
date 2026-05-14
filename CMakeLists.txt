cmake_minimum_required(VERSION 3.15)
project(hid_proxy LANGUAGES CXX)

# Must be built as a shared library (DLL)
add_library(hid SHARED dllmain.cpp)

# Use the MSVC runtime statically so the proxy has no extra runtime dependency
set_property(TARGET hid PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_compile_features(hid PRIVATE cxx_std_17)

# Output the DLL as "hid.dll"
set_target_properties(hid PROPERTIES
    OUTPUT_NAME "hid"
    PREFIX      ""
)
