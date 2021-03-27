# Prepends flags to CMAKE_C_FLAGS if supported by the C compiler. Almost all
# flags should be prepended to allow the user to override them.
macro(WEBKIT_PREPEND_GLOBAL_C_FLAGS)
    foreach (_flag ${ARGN})
        check_c_compiler_flag("${_flag}" C_COMPILER_SUPPORTS_${_flag})
        if (C_COMPILER_SUPPORTS_${_flag})
            set(CMAKE_C_FLAGS "${_flag} ${CMAKE_C_FLAGS}")
        endif ()
    endforeach ()
endmacro()

# Appends flags to CMAKE_C_FLAGS if supported by the C compiler. This macro
# should be used sparingly. Only append flags if the user must not be allowed to
# override them.
macro(WEBKIT_APPEND_GLOBAL_C_FLAGS)
    foreach (_flag ${ARGN})
        check_c_compiler_flag("${_flag}" C_COMPILER_SUPPORTS_${_flag})
        if (C_COMPILER_SUPPORTS_${_flag})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_flag}")
        endif ()
    endforeach ()
endmacro()

# Prepends flags to CMAKE_CXX_FLAGS if supported by the C++ compiler. Almost all
# flags should be prepended to allow the user to override them.
macro(WEBKIT_PREPEND_GLOBAL_CXX_FLAGS)
    foreach (_flag ${ARGN})
        check_cxx_compiler_flag("${_flag}" CXX_COMPILER_SUPPORTS_${_flag})
        if (CXX_COMPILER_SUPPORTS_${_flag})
            set(CMAKE_CXX_FLAGS "${_flag} ${CMAKE_CXX_FLAGS}")
        endif ()
    endforeach ()
endmacro()

# Appends flags to CMAKE_CXX_FLAGS if supported by the C++ compiler. This macro
# should be used sparingly. Only append flags if the user must not be allowed to
# override them.
macro(WEBKIT_APPEND_GLOBAL_CXX_FLAGS)
    foreach (_flag ${ARGN})
        check_cxx_compiler_flag("${_flag}" CXX_COMPILER_SUPPORTS_${_flag})
        if (CXX_COMPILER_SUPPORTS_${_flag})
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_flag}")
        endif ()
    endforeach ()
endmacro()

# Prepends flags to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS if supported by the C
# or C++ compiler, respectively. Almost all flags should be prepended to allow
# the user to override them.
macro(WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS)
    WEBKIT_PREPEND_GLOBAL_C_FLAGS(${ARGN})
    WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(${ARGN})
endmacro()

# Appends flags to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS if supported by the C or
# C++ compiler, respectively. This macro should be used sparingly. Only append
# flags if the user must not be allowed to override them.
macro(WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS)
    WEBKIT_APPEND_GLOBAL_C_FLAGS(${ARGN})
    WEBKIT_APPEND_GLOBAL_CXX_FLAGS(${ARGN})
endmacro()

# Appends flags to COMPILE_FLAGS of _target if supported by the C compiler.
# Note that it is simply not possible to pass different C and C++ flags, unless
# we drop support for the Visual Studio backend and use the COMPILE_LANGUAGE
# generator expression. This is a very serious limitation.
macro(WEBKIT_ADD_TARGET_C_FLAGS _target)
    foreach (_flag ${ARGN})
        check_c_compiler_flag("${_flag}" C_COMPILER_SUPPORTS_${_flag})
        if (C_COMPILER_SUPPORTS_${_flag})
            target_compile_options(${_target} PRIVATE ${_flag})
        endif ()
    endforeach ()
endmacro()

# Appends flags to COMPILE_FLAGS of _target if supported by the C++ compiler.
# Note that it is simply not possible to pass different C and C++ flags, unless
# we drop support for the Visual Studio backend and use the COMPILE_LANGUAGE
# generator expression. This is a very serious limitation.
macro(WEBKIT_ADD_TARGET_CXX_FLAGS _target)
    foreach (_flag ${ARGN})
        check_cxx_compiler_flag("${_flag}" CXX_COMPILER_SUPPORTS_${_flag})
        if (CXX_COMPILER_SUPPORTS_${_flag})
            target_compile_options(${_target} PRIVATE ${_flag})
        endif ()
    endforeach ()
endmacro()


if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(-fno-strict-aliasing)

    # clang-cl.exe impersonates cl.exe so some clang arguments like -fno-rtti are
    # represented using cl.exe's options and should not be passed as flags, so
    # we do not add -fno-rtti or -fno-exceptions for clang-cl
    if (COMPILER_IS_CLANG_CL)
        # FIXME: These warnings should be addressed
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-undef
                                             -Wno-macro-redefined
                                             -Wno-unknown-pragmas
                                             -Wno-nonportable-include-path
                                             -Wno-unknown-argument)
    else ()
        WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(-fno-exceptions)
        WEBKIT_APPEND_GLOBAL_CXX_FLAGS(-fno-rtti)

        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-tautological-compare)

        if (WIN32)
            WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(-mno-ms-bitfields)
            WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-unknown-pragmas)
            add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
        endif ()
    endif ()

    # Warnings to be enabled
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wcast-align
                                         -Wformat-security
                                         -Wmissing-format-attribute
                                         -Wpointer-arith
                                         -Wundef
                                         -Wwrite-strings)

    # Warnings to be disabled
    # FIXME: We should probably not be disabling -Wno-maybe-uninitialized?
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Qunused-arguments
                                         -Wno-maybe-uninitialized
                                         -Wno-parentheses-equality
                                         -Wno-misleading-indentation
                                         -Wno-psabi)

    WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-noexcept-type)

    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80947
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "8.0" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-attributes)
    endif ()

    # -Wexpansion-to-defined produces false positives with GCC but not Clang
    # https://bugs.webkit.org/show_bug.cgi?id=167643#c13
    if (CMAKE_COMPILER_IS_GNUCXX)
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-expansion-to-defined)
    endif ()

    # Force SSE2 fp on x86 builds.
    if (WTF_CPU_X86 AND NOT CMAKE_CROSSCOMPILING)
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-msse2 -mfpmath=sse)
        include(DetectSSE2)
        if (NOT SSE2_SUPPORT_FOUND)
            message(FATAL_ERROR "SSE2 support is required to compile WebKit")
        endif ()
    endif ()
endif ()

if (COMPILER_IS_GCC_OR_CLANG AND NOT MSVC)
    # Don't give -Wall to clang-cl because clang-cl treats /Wall and -Wall as -Weverything.
    # -Wall and -Wextra should be specified before -Wno-* for Clang.
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wall -Wextra)
endif ()

# Ninja tricks compilers into turning off color support.
if (CMAKE_GENERATOR STREQUAL "Ninja")
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-fcolor-diagnostics
                                         -fdiagnostics-color=always)
endif ()


string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND NOT "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "x86_64")
    # To avoid out of memory when building with debug option in 32bit system.
    # See https://bugs.webkit.org/show_bug.cgi?id=77327
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-Wl,--no-keep-memory ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
endif ()

if (LTO_MODE AND COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-flto=${LTO_MODE} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-flto=${LTO_MODE} ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-flto=${LTO_MODE} ${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "-flto=${LTO_MODE} ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "-flto=${LTO_MODE} ${CMAKE_MODULE_LINKER_FLAGS}")
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    # Careful: this needs to be above where ENABLED_COMPILER_SANITIZERS is set.
    # Also, it's not possible to use the normal prepend/append macros for
    # -fsanitize=* flags, because check_cxx_compiler_flag will report it's
    # unsupported, because it causes the build to fail if not used when linking.
    if (ENABLE_SANITIZERS)
        if (MSVC AND WTF_CPU_X86_64)
            find_library(CLANG_ASAN_LIBRARY clang_rt.asan_dynamic_runtime_thunk-x86_64 ${CLANG_LIB_PATH})
            find_library(CLANG_ASAN_RT_LIBRARY clang_rt.asan_dynamic-x86_64 PATHS ${CLANG_LIB_PATH})
            set(SANITIZER_LINK_FLAGS "\"${CLANG_ASAN_LIBRARY}\" \"${CLANG_ASAN_RT_LIBRARY}\"")
        else ()
            set(SANITIZER_LINK_FLAGS "-lpthread")
        endif ()

        foreach (SANITIZER ${ENABLE_SANITIZERS})
            if (${SANITIZER} MATCHES "address")
                WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS("-fno-omit-frame-pointer -fno-optimize-sibling-calls")
                set(SANITIZER_COMPILER_FLAGS "-fsanitize=address ${SANITIZER_COMPILER_FLAGS}")
                set(SANITIZER_LINK_FLAGS "-fsanitize=address ${SANITIZER_LINK_FLAGS}")

            elseif (${SANITIZER} MATCHES "undefined")
                WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS("-fno-omit-frame-pointer -fno-optimize-sibling-calls")
                # -fsanitize=vptr is incompatible with -fno-rtti
                set(SANITIZER_COMPILER_FLAGS "-fsanitize=undefined -frtti ${SANITIZER_COMPILER_FLAGS}")
                set(SANITIZER_LINK_FLAGS "-fsanitize=undefined ${SANITIZER_LINK_FLAGS}")

            elseif (${SANITIZER} MATCHES "thread" AND NOT MSVC)
                set(SANITIZER_COMPILER_FLAGS "-fsanitize=thread ${SANITIZER_COMPILER_FLAGS}")
                set(SANITIZER_LINK_FLAGS "-fsanitize=thread ${SANITIZER_LINK_FLAGS}")

            elseif (${SANITIZER} MATCHES "memory" AND COMPILER_IS_CLANG AND NOT MSVC)
                set(SANITIZER_COMPILER_FLAGS "-fsanitize=memory ${SANITIZER_COMPILER_FLAGS}")
                set(SANITIZER_LINK_FLAGS "-fsanitize=memory ${SANITIZER_LINK_FLAGS}")

            elseif (${SANITIZER} MATCHES "leak" AND NOT MSVC)
                set(SANITIZER_COMPILER_FLAGS "-fsanitize=leak ${SANITIZER_COMPILER_FLAGS}")
                set(SANITIZER_LINK_FLAGS "-fsanitize=leak ${SANITIZER_LINK_FLAGS}")

            else ()
                message(FATAL_ERROR "Unsupported sanitizer: ${SANITIZER}")
            endif ()
        endforeach ()

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_COMPILER_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_COMPILER_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
    endif ()
endif ()

if (NOT MSVC)
    string(REGEX MATCHALL "-fsanitize=[^ ]*" ENABLED_COMPILER_SANITIZERS ${CMAKE_CXX_FLAGS})
endif ()

if (UNIX AND NOT APPLE AND NOT ENABLED_COMPILER_SANITIZERS)
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined ${CMAKE_SHARED_LINKER_FLAGS}")
endif ()


if (MSVC)
    set(CODE_GENERATOR_PREPROCESSOR "\"${CMAKE_CXX_COMPILER}\" /nologo /EP /TP")
else ()
    set(CODE_GENERATOR_PREPROCESSOR "\"${CMAKE_CXX_COMPILER}\" -E -P -x c++")
endif ()


# Ensure that the default include system directories are added to the list of CMake implicit includes.
# This workarounds an issue that happens when using GCC 6 and using system includes (-isystem).
# For more details check: https://bugs.webkit.org/show_bug.cgi?id=161697
macro(DETERMINE_GCC_SYSTEM_INCLUDE_DIRS _lang _compiler _flags _result)
    file(WRITE "${CMAKE_BINARY_DIR}/CMakeFiles/dummy" "\n")
    separate_arguments(_buildFlags UNIX_COMMAND "${_flags}")
    execute_process(COMMAND ${_compiler} ${_buildFlags} -v -E -x ${_lang} -dD dummy
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/CMakeFiles OUTPUT_QUIET
                    ERROR_VARIABLE _gccOutput)
    file(REMOVE "${CMAKE_BINARY_DIR}/CMakeFiles/dummy")
    if ("${_gccOutput}" MATCHES "> search starts here[^\n]+\n *(.+) *\n *End of (search) list")
        set(${_result} ${CMAKE_MATCH_1})
        string(REPLACE "\n" " " ${_result} "${${_result}}")
        separate_arguments(${_result})
    endif ()
endmacro()

if (COMPILER_IS_GCC_OR_CLANG)
   DETERMINE_GCC_SYSTEM_INCLUDE_DIRS("c" "${CMAKE_C_COMPILER}" "${CMAKE_C_FLAGS}" SYSTEM_INCLUDE_DIRS)
   set(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES} ${SYSTEM_INCLUDE_DIRS})
   DETERMINE_GCC_SYSTEM_INCLUDE_DIRS("c++" "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_FLAGS}" SYSTEM_INCLUDE_DIRS)
   set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES} ${SYSTEM_INCLUDE_DIRS})
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    set(ATOMIC_TEST_SOURCE "
        #include <atomic>
        int main() {
          std::atomic<bool> y (false);
          std::atomic<uint64_t> x (0);
          bool expected = true;
          bool j = y.compare_exchange_weak(expected,false);
          x++;
          return 0;
        }
    ")
    check_cxx_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMICS_ARE_BUILTIN)
    if (NOT ATOMICS_ARE_BUILTIN)
        set(CMAKE_REQUIRED_LIBRARIES atomic)
        check_cxx_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMICS_REQUIRE_LIBATOMIC)
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif ()

    # <filesystem> vs <experimental/filesystem>
    set(FILESYSTEM_TEST_SOURCE "
        #include <filesystem>
        int main() { std::filesystem::path p1(\"\"); std::filesystem::status(p1); }
    ")
    set(CMAKE_REQUIRED_FLAGS "--std=c++17")
    check_cxx_source_compiles("${FILESYSTEM_TEST_SOURCE}" STD_FILESYSTEM_IS_AVAILABLE)
    if (NOT STD_FILESYSTEM_IS_AVAILABLE)
        set(EXPERIMENTAL_FILESYSTEM_TEST_SOURCE "
            #include <experimental/filesystem>
            int main() {
                std::experimental::filesystem::path p1(\"//home\");
                std::experimental::filesystem::status(p1);
            }
        ")
        set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
        check_cxx_source_compiles("${EXPERIMENTAL_FILESYSTEM_TEST_SOURCE}" STD_EXPERIMENTAL_FILESYSTEM_IS_AVAILABLE)
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif ()
    unset(CMAKE_REQUIRED_FLAGS)
endif ()
