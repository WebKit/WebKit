add_subdirectory(WGSL)

# The C++ API header. WebCore and WebKit include it as <WebGPU/WebGPUCpp.h> on every
# port that enables WebGPU, even where no WebGPU implementation is linked.
set(WebGPU_CPP_API_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPUCpp.h
)

if (APPLE)
    # Installed as a private header of WebGPU.framework.
    add_subdirectory(WebGPU)
else ()
    WEBKIT_COPY_FILES(WebGPU_CopyCppAPIHeaders
        DESTINATION ${WebGPU_PRIVATE_HEADERS_DIR}
        FILES ${WebGPU_CPP_API_HEADERS}
        FLATTENED
    )
endif ()
