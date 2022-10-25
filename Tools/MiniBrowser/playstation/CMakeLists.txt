find_library(TOOLKIT_LIBRARY ToolKitten)
set(MiniBrowser_DEPENDENCIES ToolKitten_CopyModule)

set(MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
)

set(MiniBrowser_SOURCES
    ImageButton.cpp
    MainWindow.cpp
    TitleBar.cpp
    URLBar.cpp
    WebContext.cpp
    WebViewWindow.cpp
    main.cpp
)

set(MiniBrowser_LIBRARIES
    ${C_STD_LIBRARY}
    ${KERNEL_LIBRARY}
    ${TOOLKIT_LIBRARY}
)
set(MiniBrowser_FRAMEWORKS WebKit)

WEBKIT_EXECUTABLE_DECLARE(MiniBrowser)
WEBKIT_EXECUTABLE(MiniBrowser)

target_link_options(MiniBrowser PRIVATE -Wl,--no-demangle)

if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # Set the debugger working directory for Visual Studio
    set_target_properties(MiniBrowser PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    # Set the startup target to MiniBrowser
    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MiniBrowser)
endif ()
