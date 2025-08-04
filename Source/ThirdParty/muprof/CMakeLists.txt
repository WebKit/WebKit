set_property(DIRECTORY . PROPERTY FOLDER "muprof")

file(CONFIGURE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    CONTENT "#define PACKAGE_VERSION \"0.1.0-webkit@PROJECT_VERSION@\""
    @ONLY
)

add_executable(muprof
    src/mapped-ring-buffer-source.c
    src/sysprof-controlfd-instrument.c
    src/sysprof-diagnostic.c
    src/sysprof-instrument.c
    src/sysprof-profiler.c
    src/sysprof-recording.c
    src/sysprof-spawnable.c
    src/main.c
)

target_link_libraries(muprof PRIVATE
    Dex::Dex
    SysProfCapture::SysProfCapture
    ${GLIB_LIBRARIES}
    ${GIO_UNIX_LIBRARIES}
)
target_include_directories(muprof PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
)

WEBKIT_ADD_TARGET_C_FLAGS(muprof
    -Wno-missing-field-initializers
    -Wno-sign-compare
    -Wno-unused-parameter
)

install(TARGETS muprof DESTINATION "${LIBEXEC_INSTALL_DIR}")
