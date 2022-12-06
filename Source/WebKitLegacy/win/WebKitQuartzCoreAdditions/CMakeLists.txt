cmake_minimum_required(VERSION 3.12)

include(WebKitCommon)

add_definitions(
    -DUSE_CF=1
    -DD3D_DEBUG_INFO
    -DBUILDING_WKQCA
    -DDISABLE_COREIMAGE
    -DDISABLE_FRONTEND
    -DDISABLE_IOSURFACE
    -DDISABLE_RENDERSERVER
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

set(WebKitQuartzCoreAdditions_SOURCES
    API/WKCACFImage.cpp
    API/WKCACFView.cpp
    API/WebKitQuartzCoreAdditions.cpp

    CAD3DRenderer.cpp
    CAView.cpp
    CVDisplayLink.cpp
    Image.cpp
    ImageConversion.cpp
    ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions/InvertColorPS.h
)

file(MAKE_DIRECTORY "${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions")
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions/InvertColorPS.h
    MAIN_DEPENDENCY InvertColorPS.hlsl
    COMMAND fxc /E "InvertColorPS" /T ps_2_0 /Fh "${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions/InvertColorPS.h" "${CMAKE_CURRENT_SOURCE_DIR}/InvertColorPS.hlsl"
    VERBATIM)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/API/WebKitQuartzCoreAdditionsBase.h DESTINATION ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/API/WKCACFImage.h DESTINATION ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/API/WKCACFImageInternal.h DESTINATION ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/API/WKCACFView.h DESTINATION ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/API/WKCACFViewPrivate.h DESTINATION ${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions)

include_directories(
    .
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/WebKitQuartzCoreAdditions"
    "${CMAKE_BINARY_DIR}/../include/private"
)

add_library(WebKitQuartzCoreAdditions SHARED ${WebKitQuartzCoreAdditions_SOURCES})
set_target_properties(WebKitQuartzCoreAdditions PROPERTIES OUTPUT_NAME WebKitQuartzCoreAdditions${DEBUG_SUFFIX})
target_link_libraries(WebKitQuartzCoreAdditions
    Apple::CoreFoundation
    Apple::CoreGraphics
    Apple::QuartzCore
    D3d9
    WebKit::WTF
)

target_precompile_headers(WebKitQuartzCoreAdditions PRIVATE WebKitQuartzCoreAdditionsPrefix.h)
