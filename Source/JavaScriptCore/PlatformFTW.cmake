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
    list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
        "${JAVASCRIPTCORE_DIR}/inspector/remote/socket"
    )

    list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
        inspector/remote/RemoteAutomationTarget.h
        inspector/remote/RemoteConnectionToTarget.h
        inspector/remote/RemoteControllableTarget.h
        inspector/remote/RemoteInspectionTarget.h
        inspector/remote/RemoteInspector.h

        inspector/remote/socket/RemoteInspectorConnectionClient.h
        inspector/remote/socket/RemoteInspectorMessageParser.h
        inspector/remote/socket/RemoteInspectorServer.h
        inspector/remote/socket/RemoteInspectorSocket.h
        inspector/remote/socket/RemoteInspectorSocketEndpoint.h
    )

    list(APPEND JavaScriptCore_SOURCES
        API/JSRemoteInspector.cpp

        inspector/remote/RemoteAutomationTarget.cpp
        inspector/remote/RemoteConnectionToTarget.cpp
        inspector/remote/RemoteControllableTarget.cpp
        inspector/remote/RemoteInspectionTarget.cpp
        inspector/remote/RemoteInspector.cpp

        inspector/remote/socket/RemoteInspectorConnectionClient.cpp
        inspector/remote/socket/RemoteInspectorMessageParser.cpp
        inspector/remote/socket/RemoteInspectorServer.cpp
        inspector/remote/socket/RemoteInspectorSocket.cpp
        inspector/remote/socket/RemoteInspectorSocketEndpoint.cpp

        inspector/remote/socket/win/RemoteInspectorSocketWin.cpp
    )

    list(APPEND JavaScriptCore_PRIVATE_LIBRARIES
        ws2_32
    )
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
