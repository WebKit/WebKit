if (DEVELOPER_MODE)
    find_package(Xz REQUIRED)

    configure_file(
        ${TOOLS_DIR}/wpe/manifest.txt.in
        ${CMAKE_BINARY_DIR}/manifest.txt
    )

    add_custom_target(distcheck
        COMMENT "Checking release tarball: wpewebkit-${PROJECT_VERSION}.tar"
        DEPENDS "${CMAKE_BINARY_DIR}/manifest.txt"
                "${TOOLS_DIR}/gtk/make-dist.py"
        COMMAND "${TOOLS_DIR}/gtk/make-dist.py"
                "--check" "--port=WPE"
                "--tarball-name=wpewebkit"
                "--source-dir=${CMAKE_SOURCE_DIR}"
                "--build-dir=${CMAKE_BINARY_DIR}"
                "--version=${PROJECT_VERSION}"
                "${CMAKE_BINARY_DIR}/manifest.txt"
        COMMAND "${XZ_EXECUTABLE}" "-evfQ"
                "${CMAKE_BINARY_DIR}/wpewebkit-${PROJECT_VERSION}.tar"
        USES_TERMINAL
    )

    add_custom_command(
        COMMENT "Creating release tarball: wpewebkit-${PROJECT_VERSION}.tar.xz"
        OUTPUT "${CMAKE_BINARY_DIR}/wpewebkit-${PROJECT_VERSION}.tar.xz"
        MAIN_DEPENDENCY "${CMAKE_BINARY_DIR}/manifest.txt"
        DEPENDS "${TOOLS_DIR}/gtk/make-dist.py"
        COMMAND "${TOOLS_DIR}/gtk/make-dist.py"
                "--tarball-name=wpewebkit"
                "--source-dir=${CMAKE_SOURCE_DIR}"
                "--build-dir=${CMAKE_BINARY_DIR}"
                "--version=${PROJECT_VERSION}"
                "${CMAKE_BINARY_DIR}/manifest.txt"
        COMMAND "${XZ_EXECUTABLE}" "-evfQ"
                "${CMAKE_BINARY_DIR}/wpewebkit-${PROJECT_VERSION}.tar"
        USES_TERMINAL
    )

    add_custom_target(dist
        DEPENDS "${CMAKE_BINARY_DIR}/wpewebkit-${PROJECT_VERSION}.tar.xz"
    )
endif ()
