find_library(TOOLKIT_LIBRARY ToolKitten)

PLAYSTATION_COPY_REQUIREMENTS(MiniBrowser_CopySharedLibs
    FILES ${TOOLKIT_LIBRARY}
)

set(MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    "${TOOLS_DIR}/playstation/"
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

    WebKit::WebKit
)

set(PLAYSTATION_MiniBrowser_REQUIRED_SHARED_LIBRARIES
    libToolKitten
    libcairo
    libfontconfig
    libfreetype
    libharfbuzz
    libicu
    libpng16
)

link_directories(${WEBKIT_LIBRARIES_DIR}/lib)
WEBKIT_EXECUTABLE_DECLARE(MiniBrowser)
WEBKIT_EXECUTABLE(MiniBrowser)

# MiniBrowser won't work without these processes
add_dependencies(MiniBrowser WebProcess NetworkProcess)

target_link_options(MiniBrowser PRIVATE -Wl,--no-demangle)

#
# Set the debugger working directory for Visual Studio
#
if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    set_target_properties(MiniBrowser PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()
