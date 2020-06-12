add_definitions(-DSTATICALLY_LINKED_WITH_WTF)

if (ENABLE_REMOTE_INSPECTOR)
    list(APPEND JavaScriptCore_SOURCES
        inspector/remote/RemoteConnectionToTarget.cpp
        inspector/remote/RemoteControllableTarget.cpp
        inspector/remote/RemoteInspectionTarget.cpp
        inspector/remote/RemoteInspector.cpp
    )
endif ()

if (USE_GLIB)
    if (ENABLE_REMOTE_INSPECTOR)
        list(APPEND JavaScriptCore_SOURCES
            inspector/remote/glib/RemoteInspectorGlib.cpp
        )
    endif ()

    list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND JavaScriptCore_LIBRARIES
        ${GLIB_LIBRARIES}
    )
endif ()
