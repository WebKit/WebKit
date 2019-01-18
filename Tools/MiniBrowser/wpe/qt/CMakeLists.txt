
if (ENABLE_WPE_QT_API)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    if (DEVELOPER_MODE)
        add_definitions(-DWEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    endif ()

    add_executable(qt-wpe-mini-browser "main.cpp" "qml.qrc")
    target_link_libraries(qt-wpe-mini-browser Qt5::Core Qt5::Quick)
    install(TARGETS qt-wpe-mini-browser DESTINATION "${LIBEXEC_INSTALL_DIR}")
endif ()
