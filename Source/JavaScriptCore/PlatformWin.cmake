list(APPEND JavaScriptCore_SOURCES
    API/JSStringRefBSTR.cpp
)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}/../include/private
)

if (USE_CF)
    list(APPEND JavaScriptCore_SOURCES
        API/JSStringRefCF.cpp
    )

    list(APPEND JavaScriptCore_LIBRARIES
        ${COREFOUNDATION_LIBRARY}
    )
endif ()

if (NOT WTF_PLATFORM_WIN_CAIRO)
    list(APPEND JavaScriptCore_LIBRARIES
        ${ICU_LIBRARIES}
        winmm
    )
endif ()

list(REMOVE_ITEM JavaScriptCore_SOURCES
    inspector/JSGlobalObjectInspectorController.cpp
)

file(COPY
    "${JAVASCRIPTCORE_DIR}/JavaScriptCore.vcxproj/JavaScriptCore.resources"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

WEBKIT_MAKE_FORWARDING_HEADERS(JavaScriptCore
    DIRECTORIES ${JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES}
    DERIVED_SOURCE_DIRECTORIES ${DERIVED_SOURCES_DIR}/JavaScriptCore ${DERIVED_SOURCES_DIR}/JavaScriptCore/inspector
    FLATTENED
)

set(JavaScriptCore_OUTPUT_NAME JavaScriptCore${DEBUG_SUFFIX})
