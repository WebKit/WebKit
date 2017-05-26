list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${FONTCONFIG_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
)

list(APPEND WebCore_SOURCES
    platform/graphics/freetype/FontCacheFreeType.cpp
    platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
    platform/graphics/freetype/FontPlatformDataFreeType.cpp
    platform/graphics/freetype/GlyphPageTreeNodeFreeType.cpp
    platform/graphics/freetype/SimpleFontDataFreeType.cpp

    platform/graphics/harfbuzz/ComplexTextControllerHarfBuzz.cpp
    platform/graphics/harfbuzz/HarfBuzzFace.cpp
    platform/graphics/harfbuzz/HarfBuzzShaper.cpp
)

if (USE_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/graphics/cairo/FontCairoHarfbuzzNG.cpp

        platform/graphics/harfbuzz/HarfBuzzFaceCairo.cpp
    )
endif ()

list(APPEND WebCore_LIBRARIES
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
)
