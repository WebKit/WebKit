set(MINIBROWSER_DIR "${TOOLS_DIR}/MiniBrowser/wpe")

set(MiniBrowser_SOURCES
    ${MINIBROWSER_DIR}/main.cpp
)

set(MiniBrowser_INCLUDE_DIRECTORIES
    ${TOOLS_DIR}/wpe/backends/
    ${DERIVED_SOURCES_WPE_API_DIR}
    ${FORWARDING_HEADERS_WPE_DIR}
    ${CMAKE_BINARY_DIR}/DerivedSources/WPEToolingBackends
)

set(MiniBrowser_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

set(MiniBrowser_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    WPEToolingBackends
    WebKit
    ${ATK_LIBRARIES}
    ${WPE_LIBRARIES}
    ${WPEBACKEND_FDO_LIBRARIES}
)

if (DEVELOPER_MODE)
    add_definitions(-DWEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif ()

include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${MiniBrowser_SYSTEM_INCLUDE_DIRECTORIES})
add_executable(MiniBrowser ${MiniBrowser_SOURCES})
target_link_libraries(MiniBrowser ${MiniBrowser_LIBRARIES})

install(TARGETS MiniBrowser DESTINATION "${LIBEXEC_INSTALL_DIR}")

if (ENABLE_WPE_QT_API)
    add_subdirectory(qt)
endif ()
