cmake_minimum_required(VERSION 3.14)
project(WorkflowRegistryPlugin LANGUAGES CXX)

# Include the Logos Module CMake helper
if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found")
endif()

# Universal module: the impl class IS the API. The *Plugin / *Interface glue
# is generated from src/workflow_registry_impl.h into generated_code/ by the
# Nix preConfigure (see flake.nix) and compiled in automatically.
logos_module(
    NAME workflow_registry
    SOURCES
        src/workflow_registry_impl.h
        src/workflow_registry_impl.cpp
        src/registry_bridge.h
        src/registry_bridge_lp.h
        src/registry_bridge_lp.cpp
        src/node_type_builder.h
        src/node_type_builder.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
