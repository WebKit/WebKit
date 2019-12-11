list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
)

list(APPEND WebCore_SOURCES
    platform/graphics/freetype/FontCacheFreeType.cpp
    platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
    platform/graphics/freetype/FontPlatformDataFreeType.cpp
    platform/graphics/freetype/GlyphPageTreeNodeFreeType.cpp
    platform/graphics/freetype/RefPtrFontconfig.cpp
    platform/graphics/freetype/SimpleFontDataFreeType.cpp

    platform/graphics/harfbuzz/ComplexTextControllerHarfBuzz.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/freetype/FcUniquePtr.h
    platform/graphics/freetype/RefPtrFontconfig.h

    platform/graphics/harfbuzz/HbUniquePtr.h
)

if (USE_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/graphics/cairo/FontCairoHarfbuzzNG.cpp
    )
endif ()

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${FONTCONFIG_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)

list(APPEND WebCore_LIBRARIES
    HarfBuzz::HarfBuzz
    HarfBuzz::ICU
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE_LIBRARIES}
)
