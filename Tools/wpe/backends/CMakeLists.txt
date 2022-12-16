set(WPEToolingBackends_LIBRARY_TYPE STATIC)
set(WPEToolingBackends_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/WPEToolingBackends/DerivedSources")
set(WPEToolingBackends_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/WPEToolingBackends/Headers")

file(MAKE_DIRECTORY ${WPEToolingBackends_DERIVED_SOURCES_DIR})

set(WPEToolingBackends_PUBLIC_HEADERS
    HeadlessViewBackend.h
    ViewBackend.h
)

set(WPEToolingBackends_SOURCES
    HeadlessViewBackend.cpp
    ViewBackend.cpp
)

set(WPEToolingBackends_PRIVATE_INCLUDE_DIRECTORIES
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}
    ${TOOLS_DIR}/wpe/backends
)

set(WPEToolingBackends_LIBRARIES
    WPE::libwpe
)

if (USE_CAIRO)
    list(APPEND WPEToolingBackends_LIBRARIES Cairo::Cairo)
    list(APPEND WPEToolingBackends_DEFINITIONS USE_CAIRO=1)
endif ()

set(WPEToolingBackends_INTERFACE_LIBRARIES WPEToolingBackends)
set(WPEToolingBackends_INTERFACE_INCLUDE_DIRECTORIES ${WPEToolingBackends_FRAMEWORK_HEADERS_DIR})
set(WPEToolingBackends_INTERFACE_DEPENDENCIES WPEToolingBackends_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(WPEToolingBackends)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_COPY_FILES(WPEToolingBackends_CopyHeaders
    DESTINATION ${WPEToolingBackends_FRAMEWORK_HEADERS_DIR}/WPEToolingBackends
    FILES ${WPEToolingBackends_PUBLIC_HEADERS}
    FLATTENED
)

WEBKIT_FRAMEWORK(WPEToolingBackends)
