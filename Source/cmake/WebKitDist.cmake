macro(WEBKIT_DECLARE_DIST_TARGETS _port _tarball_prefix _manifest)
    find_package(Xz REQUIRED)

    configure_file(
        ${_manifest}
        ${CMAKE_BINARY_DIR}/manifest.txt
    )

    add_custom_target(distcheck
        COMMENT "Checking release tarball: ${_tarball_prefix}-${PROJECT_VERSION}.tar"
        DEPENDS ${CMAKE_BINARY_DIR}/manifest.txt
        DEPENDS WebKit
        DEPENDS Documentation
        COMMAND ${TOOLS_DIR}/Scripts/make-dist
                --check --port=${_port}
                --tarball-name=${_tarball_prefix}
                --source-dir=${CMAKE_SOURCE_DIR}
                --build-dir=${CMAKE_BINARY_DIR}
                --version=${PROJECT_VERSION}
                ${CMAKE_BINARY_DIR}/manifest.txt
        COMMAND ${XZ_EXECUTABLE} -evfQ
                ${CMAKE_BINARY_DIR}/${_tarball_prefix}-${PROJECT_VERSION}.tar
        USES_TERMINAL
        VERBATIM
    )

    add_custom_command(
        COMMENT "Creating release tarball: ${_tarball_prefix}-${PROJECT_VERSION}.tar.xz"
        OUTPUT ${CMAKE_BINARY_DIR}/${_tarball_prefix}-${PROJECT_VERSION}.tar.xz
        MAIN_DEPENDENCY ${CMAKE_BINARY_DIR}/manifest.txt
        COMMAND ${TOOLS_DIR}/Scripts/make-dist
                --tarball-name=${_tarball_prefix}
                --source-dir=${CMAKE_SOURCE_DIR}
                --build-dir=${CMAKE_BINARY_DIR}
                --version=${PROJECT_VERSION}
                ${CMAKE_BINARY_DIR}/manifest.txt
        COMMAND ${XZ_EXECUTABLE} -evfQ
                ${CMAKE_BINARY_DIR}/${_tarball_prefix}-${PROJECT_VERSION}.tar
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(dist
        DEPENDS ${CMAKE_BINARY_DIR}/${_tarball_prefix}-${PROJECT_VERSION}.tar.xz
        DEPENDS WebKit
        DEPENDS Documentation
    )
endmacro()
