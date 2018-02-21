list(APPEND JavaScriptCore_SOURCES
    API/JSStringRefBSTR.cpp
)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSStringRefBSTR.h
    API/JavaScriptCore.h
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}/../include/private
)

if (USE_CF)
    list(APPEND JavaScriptCore_SOURCES
        API/JSStringRefCF.cpp
    )

    list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
        API/JSStringRefCF.h
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

set(JavaScriptCore_OUTPUT_NAME JavaScriptCore${DEBUG_SUFFIX})
