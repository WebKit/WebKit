list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/image-decoders"
    "${WEBCORE_DIR}/platform/image-decoders/avif"
    "${WEBCORE_DIR}/platform/image-decoders/bmp"
    "${WEBCORE_DIR}/platform/image-decoders/gif"
    "${WEBCORE_DIR}/platform/image-decoders/ico"
    "${WEBCORE_DIR}/platform/image-decoders/jpeg"
    "${WEBCORE_DIR}/platform/image-decoders/jpegxl"
    "${WEBCORE_DIR}/platform/image-decoders/png"
    "${WEBCORE_DIR}/platform/image-decoders/webp"
)

list(APPEND WebCore_SOURCES
    platform/image-decoders/ScalableImageDecoder.cpp
    platform/image-decoders/ScalableImageDecoderFrame.cpp

    platform/image-decoders/avif/AVIFImageDecoder.cpp
    platform/image-decoders/avif/AVIFImageReader.cpp

    platform/image-decoders/bmp/BMPImageDecoder.cpp
    platform/image-decoders/bmp/BMPImageReader.cpp

    platform/image-decoders/gif/GIFImageDecoder.cpp
    platform/image-decoders/gif/GIFImageReader.cpp

    platform/image-decoders/ico/ICOImageDecoder.cpp

    platform/image-decoders/jpeg/JPEGImageDecoder.cpp

    platform/image-decoders/jpegxl/JPEGXLImageDecoder.cpp

    platform/image-decoders/png/PNGImageDecoder.cpp

    platform/image-decoders/webp/WEBPImageDecoder.cpp
)

list(APPEND WebCore_LIBRARIES
    JPEG::JPEG
    PNG::PNG
)

if (WebP_FOUND)
    list(APPEND WebCore_LIBRARIES
        WebP::demux
        WebP::libwebp
    )
endif ()

if (JPEGXL_FOUND)
    list(APPEND WebCore_LIBRARIES JPEGXL::jxl)
endif ()

if (USE_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/image-decoders/cairo/ImageBackingStoreCairo.cpp
    )
endif ()

if (USE_AVIF)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/image-decoders/avif/AVIFUniquePtr.h
    )
    list(APPEND WebCore_LIBRARIES AVIF::AVIF)
endif ()
