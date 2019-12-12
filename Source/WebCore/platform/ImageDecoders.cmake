list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/image-decoders"
    "${WEBCORE_DIR}/platform/image-decoders/bmp"
    "${WEBCORE_DIR}/platform/image-decoders/gif"
    "${WEBCORE_DIR}/platform/image-decoders/ico"
    "${WEBCORE_DIR}/platform/image-decoders/jpeg"
    "${WEBCORE_DIR}/platform/image-decoders/jpeg2000"
    "${WEBCORE_DIR}/platform/image-decoders/png"
    "${WEBCORE_DIR}/platform/image-decoders/webp"
)

list(APPEND WebCore_SOURCES
    platform/image-decoders/ScalableImageDecoder.cpp
    platform/image-decoders/ScalableImageDecoderFrame.cpp

    platform/image-decoders/bmp/BMPImageDecoder.cpp
    platform/image-decoders/bmp/BMPImageReader.cpp

    platform/image-decoders/gif/GIFImageDecoder.cpp
    platform/image-decoders/gif/GIFImageReader.cpp

    platform/image-decoders/ico/ICOImageDecoder.cpp

    platform/image-decoders/jpeg/JPEGImageDecoder.cpp

    platform/image-decoders/jpeg2000/JPEG2000ImageDecoder.cpp

    platform/image-decoders/png/PNGImageDecoder.cpp

    platform/image-decoders/webp/WEBPImageDecoder.cpp
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${JPEG_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
)
list(APPEND WebCore_LIBRARIES
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
)

if (OpenJPEG_FOUND)
    list(APPEND WebCore_LIBRARIES OpenJPEG::OpenJPEG)
endif ()

if (WEBP_FOUND)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WEBP_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${WEBP_LIBRARIES}
    )
endif ()

if (USE_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/image-decoders/cairo/ImageBackingStoreCairo.cpp
    )
endif ()
