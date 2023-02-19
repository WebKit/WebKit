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
        Apple::CoreFoundation
    )
endif ()

if (ENABLE_REMOTE_INSPECTOR)
    include(inspector/remote/Socket.cmake)
else ()
    list(REMOVE_ITEM JavaScriptCore_SOURCES
        inspector/JSGlobalObjectInspectorController.cpp
    )
endif ()
