ADD_SUBDIRECTORY(${WEBKIT_DIR}/efl/DefaultTheme)

IF (ENABLE_INSPECTOR)
    ADD_CUSTOM_TARGET(
        web-inspector-resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${WEBCORE_DIR}/inspector/front-end ${WEB_INSPECTOR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${WEBCORE_DIR}/English.lproj/localizedStrings.js ${WEB_INSPECTOR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorBackendCommands.js ${WEB_INSPECTOR_DIR}/InspectorBackendCommands.js
        DEPENDS ${WebCore_LIBRARY_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    INSTALL(DIRECTORY "${CMAKE_BINARY_DIR}/${WEB_INSPECTOR_DIR}"
        DESTINATION ${DATA_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.js"
                       PATTERN "*.html"
                       PATTERN "*.css"
                       PATTERN "*.gif"
                       PATTERN "*.png")
ENDIF ()
