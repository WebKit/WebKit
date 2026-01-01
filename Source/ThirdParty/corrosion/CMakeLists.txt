cmake_minimum_required(VERSION 3.22)
project(Corrosion
    VERSION 0.6.0
    LANGUAGES NONE
    HOMEPAGE_URL "https://corrosion-rs.github.io/corrosion/"
)

# ==== Corrosion Configuration ====

option(
    CORROSION_BUILD_TESTS
    "Build Corrosion test project"
    ${PROJECT_IS_TOP_LEVEL}
)

if (PROJECT_IS_TOP_LEVEL)
    # We need to enable a language for corrosions test to work.
    # For projects using corrosion this is not needed
    enable_language(C)
endif()

# This little bit self-hosts the Corrosion toolchain to build the generator
# tool.
#
# It is strongly encouraged to install Corrosion separately and use
# `find_package(Corrosion REQUIRED)` instead if that works with your workflow.
option(CORROSION_INSTALL_ONLY "Only add rules for installing Corrosion itself." OFF)
if (NOT CORROSION_INSTALL_ONLY)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    include(Corrosion)
endif()

# Testing
if (CORROSION_BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

# If Corrosion is a subdirectory, do not enable its install code
if (NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()

# Installation

include(GNUInstallDirs)

# Generate the Config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/CorrosionConfig.cmake.in CorrosionConfig.cmake
    INSTALL_DESTINATION
        "${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Corrosion"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CorrosionConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY
        SameMajorVersion
    ARCH_INDEPENDENT
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CorrosionConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CorrosionConfigVersion.cmake"
    DESTINATION
        "${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Corrosion"
)

# These CMake scripts are needed both for the install and as a subdirectory
install(
    FILES
        cmake/Corrosion.cmake
        cmake/CorrosionGenerator.cmake
        cmake/FindRust.cmake
    DESTINATION
        "${CMAKE_INSTALL_FULL_DATADIR}/cmake"
)
