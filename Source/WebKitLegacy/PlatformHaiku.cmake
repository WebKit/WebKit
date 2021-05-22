add_custom_command(
	OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebKitVersion.h
	MAIN_DEPENDENCY ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl
	DEPENDS ${WEBKITLEGACY_DIR}/mac/Configurations/Version.xcconfig
	COMMAND ${PERL_EXECUTABLE} ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl --config ${WEBKITLEGACY_DIR}/mac/Configurations/Version.xcconfig --outputDir ${WebKitLegacy_DERIVED_SOURCES_DIR}
    VERBATIM)
list(APPEND WebKitLegacy_SOURCES ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebKitVersion.h)

LIST(APPEND WebKitLegacy_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${WEBKITLEGACY_DIR}/Storage"
    "${WEBKITLEGACY_DIR}/haiku"
    "${WEBKITLEGACY_DIR}/haiku/API"
    "${WEBKITLEGACY_DIR}/haiku/WebCoreSupport"
    "${WTF_DIR}"
    "${LIBXML2_INCLUDE_DIR}"
    "${LIBXSLT_INCLUDE_DIR}"
    "${SQLITE_INCLUDE_DIR}"
    "${CMAKE_BINARY_DIR}"
    "${FORWARDING_HEADERS_DIR}"
    /system/develop/headers/private/netservices
)

# These folders have includes with the same name as Haiku system ones. So we
# add them with -iquote only, as a way to reach the Haiku includes with
# #include <>
SET(WebKitLegacy_LOCAL_INCLUDE_DIRECTORIES
    "${FORWARDING_HEADERS_DIR}/WebCore"
    "${WEBCORE_DIR}/Modules/notifications" # Notification.h
    "${WEBCORE_DIR}/platform/text" # DateTimeFormat.h
)

foreach(inc ${WebKitLegacy_LOCAL_INCLUDE_DIRECTORIES})
    ADD_DEFINITIONS(-iquote ${inc})
endforeach(inc)

IF (ENABLE_VIDEO_TRACK)
	LIST(APPEND WebKitLegacy_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/html/track"
  )
ENDIF ()

IF (ENABLE_NOTIFICATIONS)
	LIST(APPEND WebKitLegacy_LOCAL_INCLUDE_DIRECTORIES
      "${WEBCORE_DIR}/Modules/notifications"
  )
ENDIF ()

add_definitions("-include WebKitPrefix.h")

LIST(APPEND WebKitLegacy_SOURCES
	haiku/WebCoreSupport/BackForwardList.cpp
    haiku/WebCoreSupport/ChromeClientHaiku.cpp
    haiku/WebCoreSupport/ContextMenuClientHaiku.cpp
    haiku/WebCoreSupport/DragClientHaiku.cpp
    haiku/WebCoreSupport/DumpRenderTreeSupportHaiku.cpp
    haiku/WebCoreSupport/EditorClientHaiku.cpp
    haiku/WebCoreSupport/FrameLoaderClientHaiku.cpp
    haiku/WebCoreSupport/FrameNetworkingContextHaiku.cpp
	haiku/WebCoreSupport/IconDatabase.cpp
    haiku/WebCoreSupport/InspectorClientHaiku.cpp
    haiku/WebCoreSupport/NotificationClientHaiku.cpp
    haiku/WebCoreSupport/PlatformStrategiesHaiku.cpp
    haiku/WebCoreSupport/ProgressTrackerHaiku.cpp
	haiku/WebCoreSupport/WebApplicationCache.cpp
	haiku/WebCoreSupport/WebDatabaseProvider.cpp
	haiku/WebCoreSupport/WebDiagnosticLoggingClient.cpp
    haiku/WebCoreSupport/WebVisitedLinkStore.cpp

    haiku/API/WebDownload.cpp
    haiku/API/WebDownloadPrivate.cpp
    haiku/API/WebFrame.cpp
    haiku/API/WebKitInfo.cpp
    haiku/API/WebPage.cpp
    haiku/API/WebSettings.cpp
    haiku/API/WebSettingsPrivate.cpp
    haiku/API/WebView.cpp
    haiku/API/WebWindow.cpp

    Storage/StorageAreaImpl.cpp
    Storage/StorageAreaSync.cpp
    Storage/StorageNamespaceImpl.cpp
    Storage/StorageSyncManager.cpp
    Storage/StorageThread.cpp
    Storage/StorageTracker.cpp
    Storage/WebDatabaseProvider.cpp
    Storage/WebStorageNamespaceProvider.cpp
)

LIST(APPEND WebKitLegacy_LIBRARIES
    ${LIBXML2_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${PNG_LIBRARY}
    ${JPEG_LIBRARY}
    ${CMAKE_DL_LIBS}
    be bnetapi GL shared translation tracker
    WebCore WTF
)

INSTALL(FILES
    haiku/API/WebWindow.h
    haiku/API/WebViewConstants.h
    haiku/API/WebView.h
    haiku/API/WebSettings.h
    haiku/API/WebPage.h
    haiku/API/WebKitInfo.h
    haiku/API/WebFrame.h
    haiku/API/WebDownload.h
    DESTINATION develop/headers${CMAKE_HAIKU_SECONDARY_ARCH_SUBDIR}
    COMPONENT devel
)


