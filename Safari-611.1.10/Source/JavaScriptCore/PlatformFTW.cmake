list(APPEND JavaScriptCore_SOURCES
    API/JSStringRefBSTR.cpp
)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSStringRefBSTR.h
    API/JavaScriptCore.h
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
        winmm
    )
endif ()

if (ENABLE_REMOTE_INSPECTOR)
    include(inpsector/remote/Socket.cmake)
else ()
    list(REMOVE_ITEM JavaScriptCore_SOURCES
        inspector/JSGlobalObjectInspectorController.cpp
    )
endif ()

file(COPY
    "${JAVASCRIPTCORE_DIR}/JavaScriptCore.vcxproj/JavaScriptCore.resources"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set(JavaScriptCore_OUTPUT_NAME JavaScriptCore${DEBUG_SUFFIX})
