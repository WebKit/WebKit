set(MiniBrowser_DIR "${TOOLS_DIR}/MiniBrowser/efl")

# Elementary is needed to build MiniBrowser
find_package(Elementary 1.6)

set(MiniBrowser_SOURCES
    ${TOOLS_DIR}/EWebLauncher/url_utils.c
    ${MiniBrowser_DIR}/main.c
)

set(MiniBrowser_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_FILE_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EET_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${E_DBUS_INCLUDE_DIRS}
    ${EFREET_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${ELEMENTARY_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${TOOLS_DIR}/EWebLauncher
    ${WEBKIT2_DIR}/UIProcess/API/efl
    ${WEBKIT2_DIR}
    ${CMAKE_SOURCE_DIR}/Source
)

set(MiniBrowser_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebKit2_LIBRARY_NAME}
    ${CAIRO_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_FILE_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EINA_LIBRARIES}
    ${ELEMENTARY_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GLIB_GTHREAD_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

add_definitions(-DTHEME_DIR=\"${THEME_BINARY_DIR}\")

if (ELEMENTARY_FOUND)
    include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
    add_executable(MiniBrowser ${MiniBrowser_SOURCES})
    target_link_libraries(MiniBrowser ${MiniBrowser_LIBRARIES})
    set_target_properties(MiniBrowser PROPERTIES FOLDER "Tools")
endif ()
