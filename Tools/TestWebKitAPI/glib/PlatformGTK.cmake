set(TEST_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit2GtkAPITests)
set(TEST_BINARY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WebKit2Gtk)

list(APPEND WebKitGLibAPITests_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}
    ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
    ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}
)

list(APPEND WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES
    ${ATSPI_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${GTK_UNIX_PRINT_INCLUDE_DIRS}
)

list(APPEND WebKitGLibAPITest_LIBRARIES
    ${ATSPI_LIBRARIES}
    ${GTK3_LIBRARIES}
    ${GTK_UNIX_PRINT_LIBRARIES}
)

list(APPEND WebKitGLibAPIWebProcessTests
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/AutocleanupsTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/DOMClientRectTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/DOMNodeTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/DOMNodeFilterTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/DOMXPathNSResolverTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/EditorTest.cpp
)

ADD_WK2_TEST(InspectorTestServer ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/InspectorTestServer.cpp)
ADD_WK2_TEST(TestAutocleanups ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestAutocleanups.cpp)
ADD_WK2_TEST(TestContextMenu ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestContextMenu.cpp)
ADD_WK2_TEST(TestDOMClientRect ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestDOMClientRect.cpp)
ADD_WK2_TEST(TestDOMNode ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestDOMNode.cpp)
ADD_WK2_TEST(TestDOMNodeFilter ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestDOMNodeFilter.cpp)
ADD_WK2_TEST(TestDOMXPathNSResolver ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestDOMXPathNSResolver.cpp)
ADD_WK2_TEST(TestEditor ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestEditor.cpp)
ADD_WK2_TEST(TestInspector ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestInspector.cpp)
ADD_WK2_TEST(TestInspectorServer ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestInspectorServer.cpp)
ADD_WK2_TEST(TestOptionMenu ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestOptionMenu.cpp)
ADD_WK2_TEST(TestPrinting ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestPrinting.cpp)
ADD_WK2_TEST(TestWebKitVersion ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestWebKitVersion.cpp)
ADD_WK2_TEST(TestWebViewEditor ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestWebViewEditor.cpp)

if (ATSPI_FOUND)
    ADD_WK2_TEST(AccessibilityTestServer ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/AccessibilityTestServer.cpp)
    ADD_WK2_TEST(TestWebKitAccessibility ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit2Gtk/TestWebKitAccessibility.cpp)
endif ()
