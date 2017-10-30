list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/cairo"
)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "platform/SourcesCairo.txt"
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
)

list(APPEND WebCore_LIBRARIES
    ${CAIRO_LIBRARIES}
)
