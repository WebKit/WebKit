# Checks whether all the given compiler flags are supported by the compiler.
# The _compiler may be either "C" or "CXX", and the result from the check
# will be stored in the variable named by _result.
function(WEBKIT_CHECK_COMPILER_FLAGS _compiler _result)
    string(TOUPPER "${_compiler}" _compiler)
    set(${_result} FALSE PARENT_SCOPE)
    foreach (_flag IN LISTS ARGN)
        # If an equals (=) character is present in a variable name, it will
        # not be cached correctly, and the check will be retried ad nauseam.
        string(REPLACE "=" "__" _cachevar "${_compiler}_COMPILER_SUPPORTS_${_flag}")
        if (${_compiler} STREQUAL CXX)
            check_cxx_compiler_flag("${_flag}" "${_cachevar}")
        elseif (${_compiler} STREQUAL C)
            check_c_compiler_flag("${_flag}" "${_cachevar}")
        else ()
            set(${_cachevar} FALSE CACHE INTERNAL "" FORCE)
            message(WARNING "WEBKIT_CHECK_COMPILER_FLAGS: unknown compiler '${_compiler}'")
            return()
        endif ()
        if (NOT ${_cachevar})
            return()
        endif ()
    endforeach ()
    set(${_result} TRUE PARENT_SCOPE)
endfunction()

# Appends, prepends, or sets (_op = APPEND, PREPEND, SET) flags supported
# by the C or CXX _compiler to a string _variable.
function(WEBKIT_VAR_ADD_COMPILER_FLAGS _compiler _op _variable)
    if (NOT (_op STREQUAL APPEND OR _op STREQUAL PREPEND OR _op STREQUAL SET))
        message(FATAL_ERROR "Unrecognized operation '${_op}'")
    endif ()

    if (_op STREQUAL SET)
        set(flags "")
        set(_op APPEND)
    else ()
        set(flags ${${_variable}})
    endif ()

    foreach (flag IN LISTS ARGN)
        WEBKIT_CHECK_COMPILER_FLAGS(${_compiler} flag_supported "${flag}")
        if (flag_supported)
            list(${_op} flags "${flag}")
        endif ()
    endforeach ()

    # Turn back into a plain string
    list(JOIN flags " " flags_string)
    set(${_variable} "${flags_string}" PARENT_SCOPE)
endfunction()

# Prepends flags to CMAKE_C_FLAGS if supported by the C compiler. Almost all
# flags should be prepended to allow the user to override them.
macro(WEBKIT_PREPEND_GLOBAL_C_FLAGS)
    WEBKIT_VAR_ADD_COMPILER_FLAGS(C PREPEND CMAKE_C_FLAGS ${ARGN})
endmacro()

# Appends flags to CMAKE_C_FLAGS if supported by the C compiler. This macro
# should be used sparingly. Only append flags if the user must not be allowed to
# override them.
macro(WEBKIT_APPEND_GLOBAL_C_FLAGS)
    WEBKIT_VAR_ADD_COMPILER_FLAGS(C APPEND CMAKE_C_FLAGS ${ARGN})
endmacro()

# Prepends flags to CMAKE_CXX_FLAGS if supported by the C++ compiler. Almost all
# flags should be prepended to allow the user to override them.
macro(WEBKIT_PREPEND_GLOBAL_CXX_FLAGS)
    WEBKIT_VAR_ADD_COMPILER_FLAGS(CXX PREPEND CMAKE_CXX_FLAGS ${ARGN})
endmacro()

# Appends flags to CMAKE_CXX_FLAGS if supported by the C++ compiler. This macro
# should be used sparingly. Only append flags if the user must not be allowed to
# override them.
macro(WEBKIT_APPEND_GLOBAL_CXX_FLAGS)
    WEBKIT_VAR_ADD_COMPILER_FLAGS(CXX APPEND CMAKE_CXX_FLAGS ${ARGN})
endmacro()

# Prepends flags to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS if supported by the C
# or C++ compiler, respectively. Almost all flags should be prepended to allow
# the user to override them.
macro(WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS)
    WEBKIT_VAR_ADD_COMPILER_FLAGS(C PREPEND CMAKE_C_FLAGS ${ARGN})
    WEBKIT_VAR_ADD_COMPILER_FLAGS(CXX PREPEND CMAKE_CXX_FLAGS ${ARGN})
endmacro()

# Appends flags to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS if supported by the C or
# C++ compiler, respectively. This macro should be used sparingly. Only append
# flags if the user must not be allowed to override them.
macro(WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS)
    WEBKIT_VAR_ADD_COMPILER_FLAGS(C APPEND CMAKE_C_FLAGS ${ARGN})
    WEBKIT_VAR_ADD_COMPILER_FLAGS(CXX APPEND CMAKE_CXX_FLAGS ${ARGN})
endmacro()

# Appends flags to COMPILE_OPTIONS of _subject if supported by the C
# or CXX _compiler. The _subject argument depends on its _kind, it may be
# a target name (with TARGET as _kind), or a path (with SOURCE or DIRECTORY
# as _kind).
function(WEBKIT_ADD_COMPILER_FLAGS _compiler _kind _subject)
    foreach (_flag IN LISTS ARGN)
        WEBKIT_CHECK_COMPILER_FLAGS(${_compiler} flag_supported "${_flag}")
        if (flag_supported)
            set_property(${_kind} ${_subject} APPEND PROPERTY COMPILE_OPTIONS "${_flag}")
        endif ()
    endforeach ()
endfunction()

# Appends flags to COMPILE_FLAGS of _target if supported by the C compiler.
# Note that it is simply not possible to pass different C and C++ flags, unless
# we drop support for the Visual Studio backend and use the COMPILE_LANGUAGE
# generator expression. This is a very serious limitation.
macro(WEBKIT_ADD_TARGET_C_FLAGS _target)
    WEBKIT_ADD_COMPILER_FLAGS(C TARGET ${_target} ${ARGN})
endmacro()

# Appends flags to COMPILE_FLAGS of _target if supported by the C++ compiler.
# Note that it is simply not possible to pass different C and C++ flags, unless
# we drop support for the Visual Studio backend and use the COMPILE_LANGUAGE
# generator expression. This is a very serious limitation.
macro(WEBKIT_ADD_TARGET_CXX_FLAGS _target)
    WEBKIT_ADD_COMPILER_FLAGS(CXX TARGET ${_target} ${ARGN})
endmacro()


option(DEVELOPER_MODE_FATAL_WARNINGS "Build with warnings as errors if DEVELOPER_MODE is also enabled" ON)
if (DEVELOPER_MODE AND DEVELOPER_MODE_FATAL_WARNINGS)
    if (MSVC)
        set(FATAL_WARNINGS_FLAG /WX)
    else ()
        set(FATAL_WARNINGS_FLAG -Werror)
    endif ()

    check_cxx_compiler_flag(${FATAL_WARNINGS_FLAG} CXX_COMPILER_SUPPORTS_WERROR)
    if (CXX_COMPILER_SUPPORTS_WERROR)
        set(DEVELOPER_MODE_CXX_FLAGS ${FATAL_WARNINGS_FLAG})
    endif ()
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    if (COMPILER_IS_CLANG OR (DEVELOPER_MODE AND !ARM))
        # Split debug information in ".debug_types" / ".debug_info" sections - this leads
        # to a smaller overall size of the debug information, and avoids linker relocation
        # errors on e.g. aarch64 (relocation R_AARCH64_ABS32 out of range: 4312197985 is not in [-2147483648, 4294967295])
        # But when using GCC this breaks Linux distro debuginfo generation, so limit to DEVELOPER_MODE.
        # Also when using GCC this causes ld to run out of memory on armv7, so disable for ARM.
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-fdebug-types-section)
    endif ()

    WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(-fno-strict-aliasing)

    # clang-cl.exe impersonates cl.exe so some clang arguments like -fno-rtti are
    # represented using cl.exe's options and should not be passed as flags, so
    # we do not add -fno-rtti or -fno-exceptions for clang-cl
    if (NOT COMPILER_IS_CLANG_CL)
        WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(-fno-exceptions)
        WEBKIT_APPEND_GLOBAL_CXX_FLAGS(-fno-rtti)
        WEBKIT_APPEND_GLOBAL_CXX_FLAGS(-fcoroutines)
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-fasynchronous-unwind-tables)

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
                                         -Wundef)

    # Warnings to be disabled
    # FIXME: We should probably not be disabling -Wno-maybe-uninitialized?
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Qunused-arguments
                                         -Wno-maybe-uninitialized
                                         -Wno-parentheses-equality
                                         -Wno-misleading-indentation
                                         -Wno-psabi)

    # GCC < 12.0 gives false warnings for mismatched-new-delete <https://webkit.org/b/241516>
    if ((CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0.0"))
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-mismatched-new-delete)
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-uninitialized)
    endif ()

    WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-noexcept-type)

    # These GCC warnings produce too many false positives to be useful. We'll
    # rely on developers who build with Clang to notice these warnings.
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # https://bugs.webkit.org/show_bug.cgi?id=167643#c13
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-expansion-to-defined)

        # https://bugs.webkit.org/show_bug.cgi?id=228601
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-array-bounds)
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-nonnull)

        # https://bugs.webkit.org/show_bug.cgi?id=240596
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-stringop-overflow)

        # This triggers warnings in wtf/Packed.h, a header that is included in many places. It does not
        # respect ignore warning pragmas and we cannot easily suppress it for all affected files.
        # https://bugs.webkit.org/show_bug.cgi?id=226557
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-stringop-overread)

        # -Wodr trips over our bindings integrity feature when LTO is enabled.
        # https://bugs.webkit.org/show_bug.cgi?id=229867
        WEBKIT_PREPEND_GLOBAL_CXX_FLAGS(-Wno-odr)

        # Match Clang's behavor and exit after emitting 20 errors.
        # https://bugs.webkit.org/show_bug.cgi?id=244621
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-fmax-errors=20)
    endif ()

    # Force SSE2 fp on x86 builds.
    if (WTF_CPU_X86 AND NOT CMAKE_CROSSCOMPILING)
        WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-msse2 -mfpmath=sse)
        include(DetectSSE2)
        if (NOT SSE2_SUPPORT_FOUND)
            message(FATAL_ERROR "SSE2 support is required to compile WebKit")
        endif ()
    endif ()

    # Makes builds faster. The GCC manual warns about the possibility that the assembler being
    # used may not support input from a pipe, but in practice the toolchains we support all do.
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-pipe)

    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Werror=undefined-inline
                                         -Werror=undefined-internal)
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

if (LTO_MODE AND COMPILER_IS_CLANG AND NOT MSVC)
    set(CMAKE_C_FLAGS "-flto=${LTO_MODE} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-flto=${LTO_MODE} ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-flto=${LTO_MODE} ${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "-flto=${LTO_MODE} ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "-flto=${LTO_MODE} ${CMAKE_MODULE_LINKER_FLAGS}")
elseif (LTO_MODE AND COMPILER_IS_CLANG AND MSVC AND NOT DEVELOPER_MODE)
    set(CMAKE_C_FLAGS "-flto=${LTO_MODE} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-flto=${LTO_MODE} ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "/opt:lldlto=2 ${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "/opt:lldlto=2 ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "/opt:lldlto=2 ${CMAKE_MODULE_LINKER_FLAGS}")
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
                # Please keep these options synchronized with Tools/sanitizer/ubsan.xcconfig
                WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS("-fno-omit-frame-pointer -fno-delete-null-pointer-checks -fno-optimize-sibling-calls")
                # -fsanitize=vptr is disabled because incompatible with -fno-rtti
                set(SANITIZER_COMPILER_FLAGS "-fsanitize=undefined -fno-sanitize=vptr ${SANITIZER_COMPILER_FLAGS}")
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
elseif (COMPILER_IS_QCC)
    set(CODE_GENERATOR_PREPROCESSOR "\"${CMAKE_CXX_COMPILER}\" -E -Wp,-P -x c++")
else ()
    set(CODE_GENERATOR_PREPROCESSOR "\"${CMAKE_CXX_COMPILER}\" -E -P -x c++")
endif ()


# Ensure that the default include system directories are added to the list of CMake implicit includes.
# This workarounds an issue that happens when using GCC 6 and using system includes (-isystem).
# For more details check: https://bugs.webkit.org/show_bug.cgi?id=161697
macro(DETERMINE_GCC_SYSTEM_INCLUDE_DIRS _lang _compiler _flags _result)
    file(WRITE "${CMAKE_BINARY_DIR}/CMakeFiles/dummy" "\n")
    separate_arguments(_buildFlags UNIX_COMMAND "${_flags}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env LANG=C ${_compiler} ${_buildFlags} -v -E -x ${_lang} -dD dummy
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
#include <stdbool.h>
#include <stdint.h>

#define COMPILER(FEATURE) (defined COMPILER_##FEATURE  && COMPILER_##FEATURE)

#if defined(__clang__)
#define COMPILER_CLANG 1
#endif

#if defined(__GNUC__)
#define COMPILER_GCC_COMPATIBLE 1
#endif

#if COMPILER(GCC_COMPATIBLE) && !COMPILER(CLANG)
#define COMPILER_GCC 1
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#endif

#define CPU(_FEATURE) (defined CPU_##_FEATURE && CPU_##_FEATURE)


#if COMPILER(GCC_COMPATIBLE)
/* __LP64__ is not defined on 64bit Windows since it uses LLP64. Using __SIZEOF_POINTER__ is simpler. */
#if __SIZEOF_POINTER__ == 8
#define CPU_ADDRESS64 1
#elif __SIZEOF_POINTER__ == 4
#define CPU_ADDRESS32 1
#endif
#endif

static inline bool compare_and_swap_bool_weak(bool* ptr, bool old_value, bool new_value)
{
#if COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic bool*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((bool*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

#if CPU(ADDRESS64)

typedef __uint128_t pair;

static inline bool compare_and_swap_pair_weak(void* raw_ptr, pair old_value, pair new_value)
{
#if COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((pair*)raw_ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}
#endif

static inline bool compare_and_swap_uint64_weak(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
#if COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint64_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

int main() {
    bool y = false;
    bool expected = true;
    bool j = compare_and_swap_bool_weak(&y, expected, false);
#if CPU(ADDRESS64)
    pair x = 42;
    bool k = compare_and_swap_pair_weak(&x, 42, 55);
#endif
    uint64_t z = 42;
    bool l = compare_and_swap_uint64_weak(&z, 42, 56);
    int result = (j ||
#if CPU(ADDRESS64)
                  k ||
#endif
                  l) ? 0 : 1;
    return result;
}
    ")
    check_c_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMICS_ARE_BUILTIN)
    if (NOT ATOMICS_ARE_BUILTIN)
        set(CMAKE_REQUIRED_LIBRARIES atomic)
        check_c_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMICS_REQUIRE_LIBATOMIC)
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif ()

    # <filesystem> vs <experimental/filesystem>
    set(FILESYSTEM_TEST_SOURCE "
        #include <filesystem>
        int main() { std::filesystem::path p1(\"\"); std::filesystem::status(p1); }
    ")
    set(CMAKE_REQUIRED_FLAGS "--std=c++2b")
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

if (NOT WTF_PLATFORM_COCOA)
  set(FLOAT16_TEST_SOURCE "
int main() {
  _Float16 f;

  f += static_cast<_Float16>(1.0);

  return 0;
}
  ")
  check_cxx_source_compiles("${FLOAT16_TEST_SOURCE}" HAVE_FLOAT16)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND WTF_CPU_MIPS)
    # Work around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78176.
    # This only manifests when executing 32-bit code on a 64-bit
    # processor. This is a workaround and does not cover all cases
    # (see comment #28 in the link above).
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-mno-lxc1-sxc1)
endif ()

# FIXME: Enable pre-compiled headers for all ports <https://webkit.org/b/139438>
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
