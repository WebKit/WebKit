cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)

if (POLICY CMP0042)
  cmake_policy (SET CMP0042 NEW)
endif ()

# ----------------------------------------------------------------------------
# includes
set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (utils)

# ----------------------------------------------------------------------------
# package information
set (PACKAGE_NAME      "gflags")
set (PACKAGE_VERSION   "2.2.0")
set (PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set (PACKAGE_TARNAME   "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set (PACKAGE_BUGREPORT "https://github.com/schuhschuh/gflags/issues")

project (${PACKAGE_NAME} CXX)
if (CMAKE_VERSION VERSION_LESS 100)
  # C language still needed because the following required CMake modules
  # (or their dependencies, respectively) are not correctly handling
  # the case where only CXX is enabled.
  # - CheckTypeSize.cmake (fixed in CMake 3.1, cf. http://www.cmake.org/Bug/view.php?id=14056)
  # - FindThreads.cmake (--> CheckIncludeFiles.cmake <--)
  enable_language (C)
endif ()

version_numbers (
  ${PACKAGE_VERSION}
    PACKAGE_VERSION_MAJOR
    PACKAGE_VERSION_MINOR
    PACKAGE_VERSION_PATCH
)

set (PACKAGE_SOVERSION "${PACKAGE_VERSION_MAJOR}")

# ----------------------------------------------------------------------------
# options
if (NOT GFLAGS_NAMESPACE)
  # maintain binary backwards compatibility with gflags library version <= 2.0,
  # but at the same time enable the use of the preferred new "gflags" namespace
  set (GFLAGS_NAMESPACE "google;${PACKAGE_NAME}" CACHE STRING "Name(s) of library namespace (separate multiple options by semicolon)")
  mark_as_advanced (GFLAGS_NAMESPACE)
endif ()
set (GFLAGS_NAMESPACE_SECONDARY "${GFLAGS_NAMESPACE}")
list (REMOVE_DUPLICATES GFLAGS_NAMESPACE_SECONDARY)
if (NOT GFLAGS_NAMESPACE_SECONDARY)
  message (FATAL_ERROR "GFLAGS_NAMESPACE must be set to one (or more) valid C++ namespace identifier(s separated by semicolon \";\").")
endif ()
foreach (ns IN LISTS GFLAGS_NAMESPACE_SECONDARY)
  if (NOT ns MATCHES "^[a-zA-Z][a-zA-Z0-9_]*$")
    message (FATAL_ERROR "GFLAGS_NAMESPACE contains invalid namespace identifier: ${ns}")
  endif ()
endforeach ()
list (GET       GFLAGS_NAMESPACE_SECONDARY 0 GFLAGS_NAMESPACE)
list (REMOVE_AT GFLAGS_NAMESPACE_SECONDARY 0)

option (BUILD_SHARED_LIBS          "Request build of shared libraries."                                       OFF)
option (BUILD_STATIC_LIBS          "Request build of static libraries (default if BUILD_SHARED_LIBS is OFF)." OFF)
option (BUILD_gflags_LIB           "Request build of the multi-threaded gflags library."                      ON)
option (BUILD_gflags_nothreads_LIB "Request build of the single-threaded gflags library."                     ON)
option (BUILD_PACKAGING            "Enable build of distribution packages using CPack."                       OFF)
option (BUILD_TESTING              "Enable build of the unit tests and their execution using CTest."          OFF)
option (INSTALL_HEADERS            "Request packaging of headers and other development files."                ON)

mark_as_advanced (CLEAR CMAKE_INSTALL_PREFIX)
mark_as_advanced (CMAKE_CONFIGURATION_TYPES
                  BUILD_STATIC_LIBS
                  INSTALL_HEADERS)
if (APPLE)
  mark_as_advanced(CMAKE_OSX_ARCHITECTURES
                   CMAKE_OSX_DEPLOYMENT_TARGET
                   CMAKE_OSX_SYSROOT)
endif ()

if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
  set (BUILD_STATIC_LIBS ON)
endif ()
if (NOT BUILD_gflags_LIB AND NOT BUILD_gflags_nothreads_LIB)
 message (FATAL_ERROR "At least one of BUILD_gflags_LIB and BUILD_gflags_nothreads_LIB must be ON.")
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CXX_FLAGS)
  set_property (CACHE CMAKE_BUILD_TYPE PROPERTY VALUE Release)
endif ()

if (NOT GFLAGS_INCLUDE_DIR)
  set (GFLAGS_INCLUDE_DIR "${PACKAGE_NAME}" CACHE STRING "Name of include directory of installed header files")
  mark_as_advanced (GFLAGS_INCLUDE_DIR)
else ()
  if (IS_ABSOLUTE GFLAGS_INCLUDE_DIR)
    message (FATAL_ERROR "GFLAGS_INCLUDE_DIR must be a path relative to CMAKE_INSTALL_PREFIX/include")
  endif ()
  if (GFLAGS_INCLUDE_DIR MATCHES "^\\.\\.[/\\]")
    message (FATAL_ERROR "GFLAGS_INCLUDE_DIR must not start with parent directory reference (../)")
  endif ()
endif ()

# ----------------------------------------------------------------------------
# system checks
include (CheckTypeSize)
include (CheckIncludeFileCXX)
include (CheckCXXSymbolExists)

if (WIN32 AND NOT CYGWIN)
  set (OS_WINDOWS 1)
else ()
  set (OS_WINDOWS 0)
endif ()

if (MSVC)
  set (HAVE_SYS_TYPES_H 1)
  set (HAVE_STDINT_H    1)
  set (HAVE_STDDEF_H    1) # used by CheckTypeSize module
  set (HAVE_INTTYPES_H  0)
  set (HAVE_UNISTD_H    0)
  set (HAVE_SYS_STAT_H  1)
  set (HAVE_SHLWAPI_H   1)
else ()
  foreach (fname IN ITEMS unistd stdint inttypes sys/types sys/stat fnmatch)
    string (TOUPPER "${fname}" FNAME)
    string (REPLACE "/" "_" FNAME "${FNAME}")
    if (NOT HAVE_${FNAME}_H)
      check_include_file_cxx ("${fname}.h" HAVE_${FNAME}_H)
    endif ()
  endforeach ()
  # the following are used in #if directives not #ifdef
  bool_to_int (HAVE_STDINT_H)
  bool_to_int (HAVE_SYS_TYPES_H)
  bool_to_int (HAVE_INTTYPES_H)
  if (NOT HAVE_FNMATCH_H AND OS_WINDOWS)
    check_include_file_cxx ("shlwapi.h" HAVE_SHLWAPI_H)
  endif ()
endif ()

set (GFLAGS_INTTYPES_FORMAT "" CACHE STRING "Format of integer types: \"C99\" (uint32_t), \"BSD\" (u_int32_t), \"VC7\" (__int32)")
set_property (CACHE GFLAGS_INTTYPES_FORMAT PROPERTY STRINGS "C99;BSD;VC7")
mark_as_advanced (GFLAGS_INTTYPES_FORMAT)
if (NOT GFLAGS_INTTYPES_FORMAT)
  set (TYPES uint32_t u_int32_t)
  if (MSVC)
    list (INSERT TYPES 0 __int32)
  endif ()
  foreach (type IN LISTS TYPES)
    check_type_size (${type} ${type} LANGUAGE CXX)
    if (HAVE_${type})
      break ()
    endif ()
  endforeach ()
  if (HAVE_uint32_t)
    set_property (CACHE GFLAGS_INTTYPES_FORMAT PROPERTY VALUE C99)
  elseif (HAVE_u_int32_t)
    set_property (CACHE GFLAGS_INTTYPES_FORMAT PROPERTY VALUE BSD)
  elseif (HAVE___int32)
    set_property (CACHE GFLAGS_INTTYPES_FORMAT PROPERTY VALUE VC7)
  else ()
    mark_as_advanced (CLEAR GFLAGS_INTTYPES_FORMAT)
    message (FATAL_ERROR "Do not know how to define a 32-bit integer quantity on your system!"
                         " Neither uint32_t, u_int32_t, nor __int32 seem to be available."
                         " Set GFLAGS_INTTYPES_FORMAT to either C99, BSD, or VC7 and try again.")
  endif ()
endif ()
# use of special characters in strings to circumvent bug #0008226
if ("^${GFLAGS_INTTYPES_FORMAT}$" STREQUAL "^WIN$")
  set_property (CACHE GFLAGS_INTTYPES_FORMAT PROPERTY VALUE VC7)
endif ()
if (NOT GFLAGS_INTTYPES_FORMAT MATCHES "^(C99|BSD|VC7)$")
  message (FATAL_ERROR "Invalid value for GFLAGS_INTTYPES_FORMAT! Choose one of \"C99\", \"BSD\", or \"VC7\"")
endif ()
set (GFLAGS_INTTYPES_FORMAT_C99 0)
set (GFLAGS_INTTYPES_FORMAT_BSD 0)
set (GFLAGS_INTTYPES_FORMAT_VC7 0)
set ("GFLAGS_INTTYPES_FORMAT_${GFLAGS_INTTYPES_FORMAT}" 1)

if (MSVC)
  set (HAVE_strtoll 0)
  set (HAVE_strtoq  0)
else ()
  check_cxx_symbol_exists (strtoll stdlib.h HAVE_STRTOLL)
  if (NOT HAVE_STRTOLL)
    check_cxx_symbol_exists (strtoq stdlib.h HAVE_STRTOQ)
  endif ()
endif ()

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package (Threads)
if (Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
  set (HAVE_PTHREAD 1)
  check_type_size (pthread_rwlock_t RWLOCK LANGUAGE CXX)
else ()
  set (HAVE_PTHREAD 0)
endif ()

if (UNIX AND NOT HAVE_PTHREAD AND BUILD_gflags_LIB)
  if (CMAKE_HAVE_PTHREAD_H)
    set (what "library")
  else ()
    set (what ".h file")
  endif ()
  message (FATAL_ERROR "Could not find pthread${what}. Check the log file"
                       "\n\t${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log"
                       "\nor disable the build of the multi-threaded gflags library (BUILD_gflags_LIB=OFF).")
endif ()

# ----------------------------------------------------------------------------
# source files - excluding root subdirectory and/or .in suffix
set (PUBLIC_HDRS
  "gflags.h"
  "gflags_declare.h"
  "gflags_completions.h"
)

if (GFLAGS_NAMESPACE_SECONDARY)
  set (INCLUDE_GFLAGS_NS_H "// Import gflags library symbols into alternative/deprecated namespace(s)")
  foreach (ns IN LISTS GFLAGS_NAMESPACE_SECONDARY)
    string (TOUPPER "${ns}" NS)
    set (gflags_ns_h "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/gflags_${ns}.h")
    configure_file ("${PROJECT_SOURCE_DIR}/src/gflags_ns.h.in" "${gflags_ns_h}" @ONLY)
    list (APPEND PUBLIC_HDRS "${gflags_ns_h}")
    set (INCLUDE_GFLAGS_NS_H "${INCLUDE_GFLAGS_NS_H}\n#include \"gflags_${ns}.h\"")
  endforeach ()
else ()
  set (INCLUDE_GFLAGS_NS_H)
endif ()

set (PRIVATE_HDRS
  "config.h"
  "util.h"
  "mutex.h"
)

set (GFLAGS_SRCS
  "gflags.cc"
  "gflags_reporting.cc"
  "gflags_completions.cc"
)

if (OS_WINDOWS)
  list (APPEND PRIVATE_HDRS "windows_port.h")
  list (APPEND GFLAGS_SRCS  "windows_port.cc")
endif ()

# ----------------------------------------------------------------------------
# configure source files
if (CMAKE_COMPILER_IS_GNUCXX)
  set (GFLAGS_ATTRIBUTE_UNUSED "__attribute((unused))")
else ()
  set (GFLAGS_ATTRIBUTE_UNUSED)
endif ()

# whenever we build a shared library (DLL on Windows), configure the public
# headers of the API for use of this library rather than the optionally
# also build statically linked library; users can override GFLAGS_DLL_DECL
if (BUILD_SHARED_LIBS)
  set (GFLAGS_IS_A_DLL 1)
else ()
  set (GFLAGS_IS_A_DLL 0)
endif ()

configure_headers (PUBLIC_HDRS  ${PUBLIC_HDRS})
configure_sources (PRIVATE_HDRS ${PRIVATE_HDRS})
configure_sources (GFLAGS_SRCS  ${GFLAGS_SRCS})

# ----------------------------------------------------------------------------
# output directories
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "lib")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")

# ----------------------------------------------------------------------------
# installation directories
if (OS_WINDOWS)
  set (RUNTIME_INSTALL_DIR Bin)
  set (LIBRARY_INSTALL_DIR Lib)
  set (INCLUDE_INSTALL_DIR Include)
  set (CONFIG_INSTALL_DIR  CMake)
else ()
  set (RUNTIME_INSTALL_DIR bin)
  # The LIB_INSTALL_DIR and LIB_SUFFIX variables are used by the Fedora
  # package maintainers. Also package maintainers of other distribution
  # packages need to be able to specify the name of the library directory.
  if (NOT LIB_INSTALL_DIR)
    set (LIB_INSTALL_DIR "lib${LIB_SUFFIX}")
  endif ()
  set (LIBRARY_INSTALL_DIR "${LIB_INSTALL_DIR}"
    CACHE PATH "Directory of installed libraries, e.g., \"lib64\""
  )
  mark_as_advanced (LIBRARY_INSTALL_DIR)
  set (INCLUDE_INSTALL_DIR include)
  set (CONFIG_INSTALL_DIR  ${LIBRARY_INSTALL_DIR}/cmake/${PACKAGE_NAME})
endif ()

# ----------------------------------------------------------------------------
# add library targets
set (TARGETS)
# static vs. shared
foreach (TYPE IN ITEMS STATIC SHARED)
  if (BUILD_${TYPE}_LIBS)
    # whether or not targets are a DLL
    if (OS_WINDOWS AND "^${TYPE}$" STREQUAL "^SHARED$")
      set (GFLAGS_IS_A_DLL 1)
    else ()
      set (GFLAGS_IS_A_DLL 0)
    endif ()
    string (TOLOWER "${TYPE}" type)
    # multi-threaded vs. single-threaded
    foreach (opts IN ITEMS "" _nothreads)
      if (BUILD_gflags${opts}_LIB)
        add_library (gflags${opts}-${type} ${TYPE} ${GFLAGS_SRCS} ${PRIVATE_HDRS} ${PUBLIC_HDRS})
        set (include_dirs "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>")
        if (INSTALL_HEADERS)
          list (APPEND include_dirs "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>")
        endif ()
        target_include_directories (gflags${opts}-${type}
          PUBLIC  "${include_dirs}"
          PRIVATE "${PROJECT_SOURCE_DIR}/src;${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}"
        )
        if (opts MATCHES "nothreads")
          set (defines "GFLAGS_IS_A_DLL=${GFLAGS_IS_A_DLL};NOTHREADS")
        else ()
          set (defines "GFLAGS_IS_A_DLL=${GFLAGS_IS_A_DLL}")
          if (CMAKE_USE_PTHREADS_INIT)
            target_link_libraries (gflags${opts}-${type} ${CMAKE_THREAD_LIBS_INIT})
          endif ()
        endif ()
        set_target_properties (
          gflags${opts}-${type} PROPERTIES COMPILE_DEFINITIONS "${defines}"
                                           OUTPUT_NAME         "gflags${opts}"
                                           VERSION             "${PACKAGE_VERSION}"
                                           SOVERSION           "${PACKAGE_SOVERSION}"
        )
        if (HAVE_SHLWAPI_H)
          target_link_libraries (gflags${opts}-${type} shlwapi.lib)
        endif ()
        if (NOT TARGET gflags${opts})
          add_custom_target (gflags${opts})
        endif ()
        add_dependencies (gflags${opts} gflags${opts}-${type})
        list (APPEND TARGETS gflags${opts}-${type})
      endif ()
    endforeach ()
  endif ()
endforeach ()

# ----------------------------------------------------------------------------
# installation rules
file (RELATIVE_PATH INSTALL_PREFIX_REL2CONFIG_DIR "${CMAKE_INSTALL_PREFIX}/${CONFIG_INSTALL_DIR}" "${CMAKE_INSTALL_PREFIX}")
configure_file (cmake/config.cmake.in  "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-config-install.cmake" @ONLY)
configure_file (cmake/version.cmake.in "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-config-version.cmake" @ONLY)

install (TARGETS ${TARGETS} DESTINATION ${LIBRARY_INSTALL_DIR} EXPORT gflags-lib)
if (INSTALL_HEADERS)
  install (FILES ${PUBLIC_HDRS} DESTINATION ${INCLUDE_INSTALL_DIR}/${GFLAGS_INCLUDE_DIR})
  install (
    FILES "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-config-install.cmake"
    RENAME ${PACKAGE_NAME}-config.cmake
    DESTINATION ${CONFIG_INSTALL_DIR}
  )
  install (
    FILES "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-config-version.cmake"
    DESTINATION ${CONFIG_INSTALL_DIR}
  )
  install (EXPORT gflags-lib DESTINATION ${CONFIG_INSTALL_DIR} FILE ${PACKAGE_NAME}-export.cmake)
  if (UNIX)
    install (PROGRAMS src/gflags_completions.sh DESTINATION ${RUNTIME_INSTALL_DIR})
  endif ()
endif ()

# ----------------------------------------------------------------------------
# support direct use of build tree
set (INSTALL_PREFIX_REL2CONFIG_DIR .)
export (TARGETS ${TARGETS} FILE "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-export.cmake")
export (PACKAGE gflags)
configure_file (cmake/config.cmake.in "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-config.cmake" @ONLY)

# ----------------------------------------------------------------------------
# testing - MUST follow the generation of the build tree config file
if (BUILD_TESTING)
  include (CTest)
  enable_testing ()
  add_subdirectory (test)
endif ()

# ----------------------------------------------------------------------------
# packaging
if (BUILD_PACKAGING)

  if (NOT BUILD_SHARED_LIBS AND NOT INSTALL_HEADERS)
    message (WARNING "Package will contain static libraries without headers!"
                     "\nRecommended options for generation of runtime package:"
                     "\n  BUILD_SHARED_LIBS=ON"
                     "\n  BUILD_STATIC_LIBS=OFF"
                     "\n  INSTALL_HEADERS=OFF"
                     "\nRecommended options for generation of development package:"
                     "\n  BUILD_SHARED_LIBS=ON"
                     "\n  BUILD_STATIC_LIBS=ON"
                     "\n  INSTALL_HEADERS=ON")
  endif ()

  # default package generators
  if (APPLE)
    set (PACKAGE_GENERATOR        "PackageMaker")
    set (PACKAGE_SOURCE_GENERATOR "TGZ;ZIP")
  elseif (UNIX)
    set (PACKAGE_GENERATOR        "DEB;RPM")
    set (PACKAGE_SOURCE_GENERATOR "TGZ;ZIP")
  else ()
    set (PACKAGE_GENERATOR        "ZIP")
    set (PACKAGE_SOURCE_GENERATOR "ZIP")
  endif ()

  # used package generators
  set (CPACK_GENERATOR        "${PACKAGE_GENERATOR}"        CACHE STRING "List of binary package generators (CPack).")
  set (CPACK_SOURCE_GENERATOR "${PACKAGE_SOURCE_GENERATOR}" CACHE STRING "List of source package generators (CPack).")
  mark_as_advanced (CPACK_GENERATOR CPACK_SOURCE_GENERATOR)

  # some package generators (e.g., PackageMaker) do not allow .md extension
  configure_file ("${CMAKE_CURRENT_LIST_DIR}/README.md" "${CMAKE_CURRENT_BINARY_DIR}/README.txt" COPYONLY)

  # common package information
  set (CPACK_PACKAGE_VENDOR              "Andreas Schuh")
  set (CPACK_PACKAGE_CONTACT             "google-gflags@googlegroups.com")
  set (CPACK_PACKAGE_NAME                "${PACKAGE_NAME}")
  set (CPACK_PACKAGE_VERSION             "${PACKAGE_VERSION}")
  set (CPACK_PACKAGE_VERSION_MAJOR       "${PACKAGE_VERSION_MAJOR}")
  set (CPACK_PACKAGE_VERSION_MINOR       "${PACKAGE_VERSION_MINOR}")
  set (CPACK_PACKAGE_VERSION_PATCH       "${PACKAGE_VERSION_PATCH}")
  set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "A commandline flags library that allows for distributed flags.")
  set (CPACK_RESOURCE_FILE_WELCOME       "${CMAKE_CURRENT_BINARY_DIR}/README.txt")
  set (CPACK_RESOURCE_FILE_LICENSE       "${CMAKE_CURRENT_LIST_DIR}/COPYING.txt")
  set (CPACK_PACKAGE_DESCRIPTION_FILE    "${CMAKE_CURRENT_BINARY_DIR}/README.txt")
  set (CPACK_INSTALL_PREFIX              "${CMAKE_INSTALL_PREFIX}")
  set (CPACK_OUTPUT_FILE_PREFIX          packages)
  set (CPACK_PACKAGE_RELOCATABLE         TRUE)
  set (CPACK_MONOLITHIC_INSTALL          TRUE)

  # RPM package information -- used in cmake/package.cmake.in also for DEB
  set (CPACK_RPM_PACKAGE_GROUP   "Development/Libraries")
  set (CPACK_RPM_PACKAGE_LICENSE "BSD")
  set (CPACK_RPM_PACKAGE_URL     "http://schuhschuh.github.com/gflags")
  set (CPACK_RPM_CHANGELOG_FILE  "${CMAKE_CURRENT_LIST_DIR}/ChangeLog.txt")

  if (INSTALL_HEADERS)
    set (CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_LIST_DIR}/doc/index.html")
  else ()
    set (CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_LIST_DIR}/cmake/README_runtime.txt")
  endif ()

  # system/architecture
  if (WINDOWS)
    if (CMAKE_CL_64)
      set (CPACK_SYSTEM_NAME "win64")
    else ()
      set (CPACK_SYSTEM_NAME "win32")
    endif ()
    set (CPACK_PACKAGE_ARCHITECTURE)
  elseif (APPLE)
    set (CPACK_PACKAGE_ARCHITECTURE darwin)
  else ()
    string (TOLOWER "${CMAKE_SYSTEM_NAME}" CPACK_SYSTEM_NAME)
    if (CMAKE_CXX_FLAGS MATCHES "-m32")
      set (CPACK_PACKAGE_ARCHITECTURE i386)
    else ()
      execute_process (
        COMMAND         dpkg --print-architecture
        RESULT_VARIABLE RV
        OUTPUT_VARIABLE CPACK_PACKAGE_ARCHITECTURE
      )
      if (RV EQUAL 0)
	      string (STRIP "${CPACK_PACKAGE_ARCHITECTURE}" CPACK_PACKAGE_ARCHITECTURE)
      else ()
        execute_process (COMMAND uname -m OUTPUT_VARIABLE CPACK_PACKAGE_ARCHITECTURE)
        if (CPACK_PACKAGE_ARCHITECTURE MATCHES "x86_64")
	        set (CPACK_PACKAGE_ARCHITECTURE amd64)
        else ()
          set (CPACK_PACKAGE_ARCHITECTURE i386)
        endif ()
      endif ()
    endif ()
  endif ()

  # source package settings
  set (CPACK_SOURCE_TOPLEVEL_TAG      "source")
  set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
  set (CPACK_SOURCE_IGNORE_FILES      "/\\\\.git/;\\\\.swp$;\\\\.#;/#;\\\\.*~;cscope\\\\.*;/[Bb]uild[.+-_a-zA-Z0-9]*/")

  # default binary package settings
  set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY TRUE)
  set (CPACK_PACKAGE_FILE_NAME          "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
  if (CPACK_PACKAGE_ARCHITECTURE)
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-${CPACK_PACKAGE_ARCHITECTURE}")
  endif ()

  # generator specific configuration file
  #
  # allow package maintainers to use their own configuration file
  # $ cmake -DCPACK_PROJECT_CONFIG_FILE:FILE=/path/to/package/config
  if (NOT CPACK_PROJECT_CONFIG_FILE)
    configure_file (
      "${CMAKE_CURRENT_LIST_DIR}/cmake/package.cmake.in"
      "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-package.cmake" @ONLY
    )
    set (CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-package.cmake")
  endif ()

  include (CPack)

endif () # BUILD_PACKAGING
