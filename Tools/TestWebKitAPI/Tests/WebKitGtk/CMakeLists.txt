add_definitions(-DTEST_PLUGIN_DIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Source/JavaScriptCore/ForwardingHeaders
    ${CMAKE_SOURCE_DIR}/Source/WebKit/gtk
    ${CMAKE_SOURCE_DIR}/Source/WebCore/platform/gtk
    ${CMAKE_SOURCE_DIR}/Source/WebCore/platform
    ${CMAKE_SOURCE_DIR}/Source/WTF
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_WEBKITGTK_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)

macro(ADD_WK1_TEST test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name}
        JavaScriptCore
        WebKit
        ${GLIB_LIBRARIES}
        ${GTK_LIBRARIES}
        ${LIBSOUP_LIBRARIES}
        ${X11_X11_LIB}
    )
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WebKitGtk
    )
endmacro()

ADD_WK1_TEST(testapplicationcache testapplicationcache.c)
ADD_WK1_TEST(testatkroles testatkroles.c)
ADD_WK1_TEST(testatk testatk.c)
ADD_WK1_TEST(testcontextmenu testcontextmenu.c)
ADD_WK1_TEST(testcopyandpaste testcopyandpaste.c ${WEBCORE_DIR}/platform/gtk/GtkVersioning.c)
ADD_WK1_TEST(testdomdocument testdomdocument.c)
ADD_WK1_TEST(testdomdomwindow testdomdomwindow.c)
ADD_WK1_TEST(testdomnode testdomnode.c)
ADD_WK1_TEST(testdownload testdownload.c)
ADD_WK1_TEST(testfavicondatabase testfavicondatabase.c test_utils.c)
ADD_WK1_TEST(testglobals testglobals.c)
ADD_WK1_TEST(testhittestresult testhittestresult.c)
ADD_WK1_TEST(testhttpbackend testhttpbackend.c)
ADD_WK1_TEST(testkeyevents testkeyevents.c)
ADD_WK1_TEST(testloading testloading.c)
ADD_WK1_TEST(testmimehandling testmimehandling.c test_utils.c)
ADD_WK1_TEST(testnetworkrequest testnetworkrequest.c)
ADD_WK1_TEST(testnetworkresponse testnetworkresponse.c)
ADD_WK1_TEST(testwebbackforwardlist testwebbackforwardlist.c)
ADD_WK1_TEST(testwebdatasource testwebdatasource.c)
ADD_WK1_TEST(testwebframe testwebframe.c)
ADD_WK1_TEST(testwebhistoryitem testwebhistoryitem.c)
ADD_WK1_TEST(testwebinspector testwebinspector.c test_utils.c)
ADD_WK1_TEST(testwebplugindatabase testwebplugindatabase.c)
ADD_WK1_TEST(testwebresource testwebresource.c)
ADD_WK1_TEST(testwebsettings testwebsettings.c)
ADD_WK1_TEST(testwebview testwebview.c test_utils.c)
ADD_WK1_TEST(testwindow testwindow.c)
