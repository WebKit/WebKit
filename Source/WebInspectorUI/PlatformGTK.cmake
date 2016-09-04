file(GLOB WebInspectorUI_GTK_RESOURCES
    ${WEBINSPECTORUI_DIR}/UserInterface/Images/gtk/*.png
    ${WEBINSPECTORUI_DIR}/UserInterface/Images/gtk/*.svg
)

list(APPEND WebInspectorUI_RESOURCES
    ${WebInspectorUI_GTK_RESOURCES}
)

# In developer mode, include resources that shouldn't be included in shipping builds.
if (DEVELOPER_MODE)
    list(APPEND COMMON_RESOURCE_DEFINES ENGINEERING_BUILD=1)
endif ()

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    DEPENDS ${WebInspectorUI_RESOURCES} ${TOOLS_DIR}/gtk/generate-inspector-gresource-manifest.py
    COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/gtk/generate-inspector-gresource-manifest.py --output=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml ${WebInspectorUI_RESOURCES}
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${WEBINSPECTORUI_DIR} --sourcedir=${DERIVED_SOURCES_WEBINSPECTORUI_DIR} --target=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    VERBATIM
)

# Force the bundle to be generated. WebKit2 will copy it into its own Derived Sources directory and build it.
add_custom_target(package-inspector-resources ALL
    DEPENDS ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
)