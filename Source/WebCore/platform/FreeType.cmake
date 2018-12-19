list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
)

list(APPEND WebCore_SOURCES
    platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
    platform/graphics/freetype/FontPlatformDataFreeType.cpp
    platform/graphics/freetype/GlyphPageTreeNodeFreeType.cpp
    platform/graphics/freetype/RefPtrFontconfig.cpp
    platform/graphics/freetype/SimpleFontDataFreeType.cpp

    platform/graphics/harfbuzz/ComplexTextControllerHarfBuzz.cpp
)

if (PORT STREQUAL "GTK")
    list(APPEND WebCorePlatformGTK_SOURCES
        platform/graphics/freetype/FontCacheFreeType.cpp
)
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/freetype/FontCacheFreeType.cpp
)
endif ()

if (USE_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/graphics/cairo/FontCairoHarfbuzzNG.cpp
    )
endif ()

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${FONTCONFIG_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
)

list(APPEND WebCore_LIBRARIES
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
)
