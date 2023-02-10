# This file overrides the behavior of find_package for WebKit projects.
#
# CMake provides Find modules that are used within WebKit. However there are ports
# where the default behavior does not work and they need to explicitly set their
# values. There are also targets available in later versions of CMake which are
# nicer to work with.
#
# The purpose of this file is to make the behavior of find_package consistent
# across ports and CMake versions.

# CMake provided targets. Remove wrappers whenever the minimum version is bumped.
#
# CURL::libcurl : since 3.12
# ICU::<C> : since 3.7
# Freetype::Freetype: since 3.10
# LibXml2::LibXml2: since 3.12
# LibXslt::LibXslt: since never
# JPEG::JPEG: since 3.12
# OpenSSL::SSL: Since 3.4
# PNG::PNG : since 3.4
# Threads::Threads: since 3.1
# ZLIB::ZLIB: Since 3.1

macro(find_package package)
    set(_found_package OFF)

    # Apple builds have a unique location for ICU
    if (USE_APPLE_ICU AND "${package}" STREQUAL "ICU")
        set(_found_package ON)

        set(ICU_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/ICU/Headers)

        # Apple just has a single tbd/dylib for ICU.
        find_library(ICU_I18N_LIBRARY icucore)
        find_library(ICU_UC_LIBRARY icucore)
        find_library(ICU_DATA_LIBRARY icucore)

        set(ICU_LIBRARIES ${ICU_UC_LIBRARY})
        set(ICU_FOUND ON)
        message(STATUS "Found ICU: ${ICU_LIBRARIES}")
    endif ()

    if (NOT _found_package)
        _find_package(${ARGV})
    endif ()

    # Create targets that are present in later versions of CMake or are referenced above
    if ("${package}" STREQUAL "CURL")
        if (CURL_FOUND AND NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl UNKNOWN IMPORTED)
            set_target_properties(CURL::libcurl PROPERTIES
                IMPORTED_LOCATION "${CURL_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "ICU")
        if (ICU_FOUND AND NOT TARGET ICU::data)
            add_library(ICU::data UNKNOWN IMPORTED)
            set_target_properties(ICU::data PROPERTIES
                IMPORTED_LOCATION "${ICU_DATA_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            )

            add_library(ICU::i18n UNKNOWN IMPORTED)
            set_target_properties(ICU::i18n PROPERTIES
                IMPORTED_LOCATION "${ICU_I18N_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            )

            add_library(ICU::uc UNKNOWN IMPORTED)
            set_target_properties(ICU::uc PROPERTIES
                IMPORTED_LOCATION "${ICU_UC_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            )
        endif ()
    elseif ("${package}" STREQUAL "LibXml2")
        if (LIBXML2_FOUND AND NOT TARGET LibXml2::LibXml2)
            add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
            set_target_properties(LibXml2::LibXml2 PROPERTIES
                IMPORTED_LOCATION "${LIBXML2_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
                INTERFACE_COMPILE_OPTIONS "${LIBXML2_DEFINITIONS}"
            )
        endif ()
    elseif ("${package}" STREQUAL "LibXslt")
        if (LIBXSLT_FOUND AND NOT TARGET LibXslt::LibXslt)
            add_library(LibXslt::LibXslt UNKNOWN IMPORTED)
            set_target_properties(LibXslt::LibXslt PROPERTIES
                IMPORTED_LOCATION "${LIBXSLT_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXSLT_INCLUDE_DIR}"
                INTERFACE_COMPILE_OPTIONS "${LIBXSLT_DEFINITIONS}"
            )
        endif ()
        if (LIBXSLT_EXSLT_LIBRARY AND NOT TARGET LibXslt::LibExslt)
            add_library(LibXslt::LibExslt UNKNOWN IMPORTED)
            set_target_properties(LibXslt::LibExslt PROPERTIES
                IMPORTED_LOCATION "${LIBXSLT_EXSLT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXSLT_INCLUDE_DIR}"
            )
        endif ()
    elseif ("${package}" STREQUAL "JPEG")
        if (JPEG_FOUND AND NOT TARGET JPEG::JPEG)
            add_library(JPEG::JPEG UNKNOWN IMPORTED)
            set_target_properties(JPEG::JPEG PROPERTIES
                IMPORTED_LOCATION "${JPEG_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}"
            )
        endif ()
    elseif ("${package}" STREQUAL "ZLIB")
        if (ZLIB_FOUND AND NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
            set_target_properties(ZLIB::ZLIB PROPERTIES
                IMPORTED_LOCATION "${ZLIB_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
            )
        endif ()
    endif ()
endmacro()
