# WPE Mock Platform
set(WPEPlatformMock_SOURCES
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/mock/WPEDisplayMock.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/mock/WPEScreenMock.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/mock/WPEToplevelMock.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/mock/WPEViewMock.cpp
)

set(WPEPlatformMock_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WPEPlatform_DERIVED_SOURCES_DIR}
    ${WEBKIT_DIR}/WPEPlatform
    ${WTF_FRAMEWORK_HEADERS_DIR}
)

set(WPEPlatformMock_LIBRARIES
    LibDRM::LibDRM
    WTF
    WebKit
    bmalloc
)

add_library(WPEPlatformMock STATIC ${WPEPlatformMock_SOURCES})
target_include_directories(WPEPlatformMock PRIVATE ${WPEPlatformMock_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatformMock SYSTEM PRIVATE ${WPEPlatformMock_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEPlatformMock ${WPEPlatformMock_LIBRARIES})

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WPEPlatformMock -Wno-unused-parameter)
endif ()

# WPE Mock Platform Module
set(WPEPlatformMockModule_SOURCES
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/mock/WPEPlatformMockModule.cpp
)

set(WPEPlatformMockModule_PRIVATE_INCLUDE_DIRECTORIES
    ${WPEPlatform_DERIVED_SOURCES_DIR}
    ${WEBKIT_DIR}/WPEPlatform
)

set(WPEPlatformMockModule_LIBRARIES
    WPEPlatformMock
)

add_library(WPEPlatformMockModule MODULE ${WPEPlatformMockModule_SOURCES})
set_target_properties(WPEPlatformMockModule PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WPEPlatformMock)
target_include_directories(WPEPlatformMockModule PRIVATE ${WPEPlatformMockModule_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatformMockModule SYSTEM PRIVATE ${WPEPlatformMockModule_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEPlatformMockModule ${WPEPlatformMockModule_LIBRARIES})

# WPE Platform Tests
set(WPEPlatformTest_SOURCES
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/WPEMockPlatformTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/WPEPlatformTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/WPEPlatformTestMain.cpp
)

if (ENABLE_WPE_PLATFORM_WAYLAND)
    list(APPEND WPEPlatformTest_SOURCES
        ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/WPEWaylandPlatformTest.cpp
    )
endif ()

set(WPEPlatformTest_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform
    ${TOOLS_DIR}/TestWebKitAPI/glib/WPEPlatform/mock
    ${WPEPlatform_DERIVED_SOURCES_DIR}
    ${WEBKIT_DIR}/WPEPlatform
    ${WTF_FRAMEWORK_HEADERS_DIR}
)

set(WPEPlatformTest_DEFINITIONS
    NO_GTEST_USAGE
    WPE_MOCK_PLATFORM_DIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WPEPlatformMock"
)

set(WPEPlatformTest_LIBRARIES
    WPEPlatformMock
)

add_library(WPEPlatformTest STATIC ${WPEPlatformTest_SOURCES})
target_include_directories(WPEPlatformTest PRIVATE ${WPEPlatformTest_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatformTest SYSTEM PRIVATE ${WPEPlatformTest_SYSTEM_INCLUDE_DIRECTORIES})
target_compile_definitions(WPEPlatformTest PUBLIC ${WPEPlatformTest_DEFINITIONS})
target_link_libraries(WPEPlatformTest ${WPEPlatformTest_LIBRARIES})

macro(ADD_WPE_PLATFORM_TEST test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} WPEPlatformTest)
    target_include_directories(${test_name} PRIVATE ${WPEPlatformTest_PRIVATE_INCLUDE_DIRECTORIES})
    target_include_directories(${test_name} SYSTEM PRIVATE ${WPEPlatformTest_SYSTEM_INCLUDE_DIRECTORIES})

    set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WPEPlatform)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${test_name} -Wno-unused-parameter)
    endif ()
endmacro()

ADD_WPE_PLATFORM_TEST(TestDisplay ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEPlatform/TestDisplay.cpp)
ADD_WPE_PLATFORM_TEST(TestDisplayDefault ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEPlatform/TestDisplayDefault.cpp)
ADD_WPE_PLATFORM_TEST(TestSettings ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEPlatform/TestSettings.cpp)
ADD_WPE_PLATFORM_TEST(TestView ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEPlatform/TestView.cpp)

if (ENABLE_WPE_PLATFORM_WAYLAND)
    ADD_WPE_PLATFORM_TEST(TestDisplayWayland ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEPlatform/TestDisplayWayland.cpp)
endif ()
