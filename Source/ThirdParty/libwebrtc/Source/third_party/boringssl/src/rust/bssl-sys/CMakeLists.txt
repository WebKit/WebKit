# Generate architecture-specific wrappers. bindgen must be called from
# ${CMAKE_BINARY_DIR}, with the output path as a relative path. bindgen writes
# the depfile using the same syntax as the command-line argument, and ninja
# requires a path relative to the top-level build directory.
set(wrapper_rs wrapper_${RUST_BINDINGS}.rs)
binary_dir_relative_path(${wrapper_rs} wrapper_rs_relative)
binary_dir_relative_path(${wrapper_rs}.d depfile_relative)

add_custom_command(
  OUTPUT ${wrapper_rs} wrapper.c
  COMMAND ${BINDGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/wrapper.h
          -o ${wrapper_rs_relative}
          --depfile=${depfile_relative}
          --no-derive-default
          --enable-function-attribute-detection
          --use-core
          --default-macro-constant-type=signed
          --rustified-enum=point_conversion_form_t
          # These regexes need to accept both / and \ to handle Windows file
          # path differences, due a bindgen issue. See
          # https://crbug.com/boringssl/595. Ideally, we would write [/\\], but
          # there are many layers of escaping here. First, CMake interprets
          # backslashes. Then CMake generates a Ninja or Make file. That, in
          # turn, uses the shell on POSIX, and does something else on Windows.
          #
          # It is unlikely that every layer here has sufficiently well-defined
          # escaping and correctly handled the next layer's escaping. On top of
          # that, we'd likely need to detect Windows vs POSIX hosts and change
          # the input. Instead, just use [[:punct:]] which is more permissive
          # than necessary, but we only need to exclude unwanted libc headers.
          #
          # If bindgen ever supports some file-based config (see
          # https://github.com/rust-lang/rust-bindgen/issues/2508), we can
          # switch to that.
          --allowlist-file=".*[[:punct:]]include[[:punct:]]openssl[[:punct:]].*\\.h"
          --experimental
          --wrap-static-fns
          --wrap-static-fns-path="${CMAKE_CURRENT_BINARY_DIR}/wrapper.c"
          -- # these are LLVM arg passthroughs
          -I${PROJECT_SOURCE_DIR}/include
          # https://doc.rust-lang.org/nightly/rustc/platform-support.html
          --target=${RUST_BINDINGS}
  DEPENDS wrapper.h
  DEPFILE ${CMAKE_CURRENT_BINARY_DIR}/${wrapper_rs}.d
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_library(rust_wrapper STATIC wrapper.c)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(rust_wrapper PRIVATE "-Wno-missing-prototypes")
endif()
target_include_directories(rust_wrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rust_wrapper crypto ssl)

add_custom_target(bssl_sys ALL DEPENDS ${wrapper_rs} rust_wrapper)
