set_property(DIRECTORY . PROPERTY FOLDER "libdex")

#
# Generating dex-version.h from the included template needs using variable
# names which may be already in use: save them and restore their values
# afterwards.
#
set(Dex_SAVED_VERSION "${VERSION}")
set(Dex_SAVED_MAJOR_VERSION "${MAJOR_VERSION}")
set(Dex_SAVED_MINOR_VERSION "${MINOR_VERSION}")
set(Dex_SAVED_MICRO_VERSION "${MICRO_VERSION}")

set(MAJOR_VERSION 0)
set(MINOR_VERSION 8)
set(MICRO_VERSION 1)
set(VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}")
set(Dex_VERSION "${VERSION}")

configure_file(src/dex-version.h.in "${CMAKE_CURRENT_BINARY_DIR}/dex-version.h" @ONLY)

set(VERSION "${Dex_SAVED_VERSION}")
set(MAJOR_VERSION "${Dex_SAVED_MAJOR_VERSION}")
set(MINOR_VERSION "${Dex_SAVED_MINOR_VERSION}")
set(MICRO_VERSION "${Dex_SAVED_MICRO_VERSION}")

set(Dex_HAVE_LIBURING FALSE)

find_package(PkgConfig QUIET)
pkg_check_modules(LibUring liburing>=0.7 IMPORTED_TARGET)
if (TARGET PkgConfig::LibUring)
    set(Dex_HAVE_LIBURING TRUE)
endif ()

WEBKIT_CHECK_HAVE_INCLUDE(HAVE_UCONTEXT_H ucontext.h)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_EVENTFD eventfd sys/eventfd.h)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    if (WTF_CPU_ARM64)
        set(Dex_ALIGN_OF_UCONTEXT 16)
    else ()
        set(Dex_ALIGN_OF_UCONTEXT 8)
    endif ()
elseif (WIN32)
    message(FATAL_ERROR "bundled libdex is unsupported on Windows")
else ()
    message(CHECK_START "Determining alignment of ucontext_t")
    #
    # The libdex build system uses the Meson compiler.alignment() function, which
    # is not available in CMake. An alternative way of calculating that is packing
    # a one-byte member in a struct followed by an ucontext_t, then picking the
    # offset. This is the same as the struct having *exactly* those two fields,
    # and checking how much padding the compiler adds, which is the size of the
    # struct minus the size of ucontext_t.
    #
    set(Dex_TEST_UCONTEXT_SIZES_HEADER
        "struct padding_and_ucontext { char pad; ucontext_t ctx; };"
    )
    if (HAVE_UCONTEXT_H)
        string(PREPEND Dex_TEST_UCONTEXT_SIZES_HEADER "#include <ucontext.h>\n")
    endif ()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test-ucontext-sizes.h"
        "${Dex_TEST_UCONTEXT_SIZES_HEADER}"
    )

    set(Dex_SAVED_EXTRA_INCLUDE_FILES ${CMAKE_EXTRA_INCLUDE_FILES})
    set(Dex_SAVED_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
    set(Dex_SAVED_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET})

    set(CMAKE_EXTRA_INCLUDE_FILES "test-ucontext-sizes.h")
    set(CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
    set(CMAKE_REQUIRED_QUIET TRUE)

    check_type_size("ucontext_t" Dex_SIZEOF_UCONTEXT_T LANGUAGE C)
    check_type_size("struct padding_and_ucontext" Dex_SIZEOF_PADDING_AND_UCONTEXT LANGUAGE C)

    if (NOT Dex_SIZEOF_UCONTEXT_T)
        message(FATAL_ERROR "Cannot determine sizeof(ucontext_t)")
    endif ()

    set(CMAKE_EXTRA_INCLUDE_FILES ${Dex_SAVED_EXTRA_INCLUDE_FILES})
    set(CMAKE_REQUIRED_INCLUDES ${Dex_SAVED_REQUIRED_INCLUDES})
    set(CMAKE_REQUIRED_QUIET} ${Dex_SAVED_REQUIRED_QUIET})

    math(EXPR Dex_ALIGN_OF_UCONTEXT "${Dex_SIZEOF_PADDING_AND_UCONTEXT} - ${Dex_SIZEOF_UCONTEXT_T}")
    message(CHECK_PASS "${Dex_ALIGN_OF_UCONTEXT}")
endif ()

add_library(Dex STATIC
    src/dex-aio.c
    src/dex-aio-backend.c
    src/dex-async-pair.c
    src/dex-async-result.c
    src/dex-block.c
    src/dex-cancellable.c
    src/dex-channel.c
    src/dex-error.c
    src/dex-fiber.c
    src/dex-future-set.c
    src/dex-future.c
    src/dex-gio.c
    src/dex-infinite.c
    src/dex-init.c
    src/dex-main-scheduler.c
    src/dex-object.c
    src/dex-platform.c
    src/dex-posix-aio-backend.c
    src/dex-posix-aio-future.c
    src/dex-promise.c
    src/dex-scheduler.c
    src/dex-semaphore.c
    src/dex-stack.c
    src/dex-static-future.c
    src/dex-thread-pool-scheduler.c
    src/dex-thread-pool-worker.c
    src/dex-thread-storage.c
    src/dex-timeout.c
    src/dex-unix-signal.c
    src/dex-work-queue.c
    src/dex-work-stealing-queue.c
)

WEBKIT_ADD_TARGET_C_FLAGS(Dex
    -Wno-atomic-alignment
    -Wno-cast-function-type
    -Wno-sign-compare
    -Wno-unused-parameter
)

target_compile_definitions(Dex PRIVATE
    G_LOG_DOMAIN=\"Dex\"
    DEX_COMPILATION
)
if (WTF_OS_DARWIN)
    target_compile_definitions(Dex PRIVATE _XOPEN_SOURCE)
endif ()

target_link_libraries(Dex PRIVATE
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
)

target_include_directories(Dex PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(Dex PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE TRUE
    VISIBILITY_INLINES_HIDDEN TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src;${CMAKE_CURRENT_BINARY_DIR}"
)

if (NOT WTF_OS_DARWIN AND (NOT WTF_OS_LINUX OR WTF_CPU_MIPS OR WTF_CPU_MIPS64))
    target_sources(Dex PRIVATE src/asm.S)
endif ()

if (ATOMICS_REQUIRE_LIBATOMIC)
    target_link_libraries(Dex PRIVATE atomic)
endif ()

configure_file(cmake-config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)

if (Dex_HAVE_LIBURING)
    set(DEX_URING_MAJOR_VERSION 0)
    set(DEX_URING_MINOR_VERSION 0)
    if (LibUring_VERSION)
        string(REGEX MATCH "([0-9]+)\.([0-9]+)" _dummy "${LibUring_VERSION}")
        set(DEX_URING_MAJOR_VERSION "${CMAKE_MATCH_1}")
        set(DEX_URING_MINOR_VERSION "${CMAKE_MATCH_2}")
    endif ()
    configure_file(src/dex-uring-version.h.in "${CMAKE_CURRENT_BINARY_DIR}/dex-uring-version.h" @ONLY)

    target_link_libraries(Dex PRIVATE PkgConfig::LibUring)
    target_sources(Dex PRIVATE
        src/dex-uring-aio-backend.c
        src/dex-uring-future.c
    )
endif ()

add_library(Dex::Dex ALIAS Dex)
