list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "inspector/remote/SourcesGLib.txt"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/inspector/remote/glib"
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/glib/RemoteInspectorServer.h
    inspector/remote/glib/RemoteInspectorUtils.h
)

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)
list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
