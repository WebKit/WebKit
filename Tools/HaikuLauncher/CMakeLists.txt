SET(HaikuLauncher_SOURCES
    ${TOOLS_DIR}/HaikuLauncher/LauncherApp.cpp
    ${TOOLS_DIR}/HaikuLauncher/LauncherWindow.cpp
    ${TOOLS_DIR}/HaikuLauncher/AuthenticationPanel.cpp
)

SET(HaikuLauncher_LIBRARIES
    JavaScriptCore
    WebCore
	WebKitLegacy
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${ICU_I18N_LIBRARIES}
    ${ICU_LIBRARIES}
    be bsd network stdc++ translation tracker
)

INCLUDE_DIRECTORIES("${WEBKITLEGACY_DIR}/haiku/API")

ADD_EXECUTABLE(HaikuLauncher ${HaikuLauncher_SOURCES})
TARGET_LINK_LIBRARIES(HaikuLauncher ${HaikuLauncher_LIBRARIES})
SET_TARGET_PROPERTIES(HaikuLauncher PROPERTIES FOLDER "Tools")
SET_TARGET_PROPERTIES(HaikuLauncher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
