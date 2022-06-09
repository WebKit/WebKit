set(TEST_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit2GtkAPITests)
set(TEST_BINARY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WebKit2Gtk)

list(APPEND WebKitGLibAPITests_SOURCES
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/gtk/WebViewTestGtk.cpp
)

list(APPEND WebKitGLibAPITests_INCLUDE_DIRECTORIES
    ${WebKit2Gtk_DERIVED_SOURCES_DIR}
    ${WebKit2Gtk_FRAMEWORK_HEADERS_DIR}
    ${WebKit2Gtk_FRAMEWORK_HEADERS_DIR}/webkit2gtk-${WEBKITGTK_API_VERSION}
    ${WebKit2Gtk_FRAMEWORK_HEADERS_DIR}/webkit2gtk-webextension
)

list(APPEND WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES
    ${ATSPI_INCLUDE_DIRS}
)

list(APPEND WebKitGLibAPITest_LIBRARIES
    ${ATSPI_LIBRARIES}
    GTK::GTK
)

if (GTK_UNIX_PRINT_FOUND)
    list(APPEND WebKitGLibAPITest_LIBRARIES GTK::UnixPrint)
endif ()

list(APPEND WebKitGLibAPIWebProcessTests
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/AutocleanupsTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/DOMClientRectTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/DOMNodeTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/DOMNodeFilterTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/DOMXPathNSResolverTest.cpp
)

ADD_WK2_TEST(InspectorTestServer ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/InspectorTestServer.cpp)
ADD_WK2_TEST(TestAutocleanups ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestAutocleanups.cpp)
ADD_WK2_TEST(TestContextMenu ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestContextMenu.cpp)
ADD_WK2_TEST(TestDOMClientRect ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestDOMClientRect.cpp)
ADD_WK2_TEST(TestDOMNode ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestDOMNode.cpp)
ADD_WK2_TEST(TestDOMNodeFilter ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestDOMNodeFilter.cpp)
ADD_WK2_TEST(TestDOMXPathNSResolver ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestDOMXPathNSResolver.cpp)
ADD_WK2_TEST(TestInspector ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestInspector.cpp)
ADD_WK2_TEST(TestInspectorServer ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestInspectorServer.cpp)
ADD_WK2_TEST(TestPrinting ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestPrinting.cpp)
ADD_WK2_TEST(TestWebKitFaviconDatabase ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitFaviconDatabase.cpp)
ADD_WK2_TEST(TestWebKitVersion ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestWebKitVersion.cpp)
ADD_WK2_TEST(TestWebViewEditor ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestWebViewEditor.cpp)

if (ATSPI_FOUND)
    ADD_WK2_TEST(TestWebKitAccessibility ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGtk/TestWebKitAccessibility.cpp)
endif ()
