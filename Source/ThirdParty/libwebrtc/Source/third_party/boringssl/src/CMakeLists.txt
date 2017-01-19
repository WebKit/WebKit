cmake_minimum_required (VERSION 2.8.10)

# Defer enabling C and CXX languages.
project (BoringSSL NONE)

if(WIN32)
  # On Windows, prefer cl over gcc if both are available. By default most of
  # the CMake generators prefer gcc, even on Windows.
  set(CMAKE_GENERATOR_CC cl)
endif()

enable_language(C)
enable_language(CXX)

if(ANDROID)
  # Android-NDK CMake files reconfigure the path and so Go and Perl won't be
  # found. However, ninja will still find them in $PATH if we just name them.
  if(NOT PERL_EXECUTABLE)
    set(PERL_EXECUTABLE "perl")
  endif()
  if(NOT GO_EXECUTABLE)
    set(GO_EXECUTABLE "go")
  endif()
else()
  find_package(Perl REQUIRED)
  find_program(GO_EXECUTABLE go)
endif()

if (NOT GO_EXECUTABLE)
  message(FATAL_ERROR "Could not find Go")
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(C_CXX_FLAGS "-Wall -Werror -Wformat=2 -Wsign-compare -Wmissing-field-initializers -Wwrite-strings -ggdb -fvisibility=hidden -fno-common")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wnewline-eof")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS} -Wmissing-prototypes -Wold-style-definition -Wstrict-prototypes")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${C_CXX_FLAGS} -Wmissing-declarations")
  # Clang's integerated assembler does not support debug symbols.
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-g")
  endif()
elseif(MSVC)
  set(MSVC_DISABLED_WARNINGS_LIST
      "C4100" # 'exarg' : unreferenced formal parameter
      "C4127" # conditional expression is constant
      "C4200" # nonstandard extension used : zero-sized array in
              # struct/union.
      "C4242" # 'function' : conversion from 'int' to 'uint8_t',
              # possible loss of data
      "C4244" # 'function' : conversion from 'int' to 'uint8_t',
              # possible loss of data
      "C4245" # 'initializing' : conversion from 'long' to
              # 'unsigned long', signed/unsigned mismatch
      "C4267" # conversion from 'size_t' to 'int', possible loss of data
      "C4371" # layout of class may have changed from a previous version of the
              # compiler due to better packing of member '...'
      "C4388" # signed/unsigned mismatch
      "C4296" # '>=' : expression is always true
      "C4350" # behavior change: 'std::_Wrap_alloc...'
      "C4365" # '=' : conversion from 'size_t' to 'int',
              # signed/unsigned mismatch
      "C4389" # '!=' : signed/unsigned mismatch
      "C4464" # relative include path contains '..'
      "C4510" # 'argument' : default constructor could not be generated
      "C4512" # 'argument' : assignment operator could not be generated
      "C4514" # 'function': unreferenced inline function has been removed
      "C4548" # expression before comma has no effect; expected expression with
              # side-effect" caused by FD_* macros.
      "C4610" # struct 'argument' can never be instantiated - user defined
              # constructor required.
      "C4623" # default constructor was implicitly defined as deleted
      "C4625" # copy constructor could not be generated because a base class
              # copy constructor is inaccessible or deleted
      "C4626" # assignment operator could not be generated because a base class
              # assignment operator is inaccessible or deleted
      "C4706" # assignment within conditional expression
      "C4710" # 'function': function not inlined
      "C4711" # function 'function' selected for inline expansion
      "C4800" # 'int' : forcing value to bool 'true' or 'false'
              # (performance warning)
      "C4820" # 'bytes' bytes padding added after construct 'member_name'
      "C5027" # move assignment operator was implicitly defined as deleted
      )
  set(MSVC_LEVEL4_WARNINGS_LIST
      # See https://connect.microsoft.com/VisualStudio/feedback/details/1217660/warning-c4265-when-using-functional-header
      "C4265" # class has virtual functions, but destructor is not virtual
      )
  string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
                            ${MSVC_DISABLED_WARNINGS_LIST})
  string(REPLACE "C" " -w4" MSVC_LEVEL4_WARNINGS_STR
                            ${MSVC_LEVEL4_WARNINGS_LIST})
  set(CMAKE_C_FLAGS   "-Wall -WX ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
  set(CMAKE_CXX_FLAGS "-Wall -WX ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
  set(CMAKE_ASM_NASM_FLAGS "-g cv8")
  add_definitions(-D_HAS_EXCEPTIONS=0)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-DNOMINMAX)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS) # Allow use of fopen
endif()

if((CMAKE_COMPILER_IS_GNUCXX AND CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.7.99") OR
   CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  if ((CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.8.99") OR
      CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  endif()
endif()

# pthread_rwlock_t requires a feature flag.
if(NOT WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
endif()

if(FUZZ)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "You need to build with Clang for fuzzing to work")
  endif()

  add_definitions(-DBORINGSSL_UNSAFE_FUZZER_MODE)
  set(RUNNER_ARGS "-fuzzer" "-deterministic" "-shim-config" "fuzzer_mode.json")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-coverage=edge,indirect-calls,8bit-counters")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-coverage=edge,indirect-calls,8bit-counters")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  link_directories(.)
endif()

add_definitions(-DBORINGSSL_IMPLEMENTATION)

if (BUILD_SHARED_LIBS)
  add_definitions(-DBORINGSSL_SHARED_LIBRARY)
  # Enable position-independent code globally. This is needed because
  # some library targets are OBJECT libraries.
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(ARCH "x86_64")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
  set(ARCH "x86_64")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  # cmake reports AMD64 on Windows, but we might be building for 32-bit.
  if (CMAKE_CL_64)
    set(ARCH "x86_64")
  else()
    set(ARCH "x86")
  endif()
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
  set(ARCH "x86")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
  set(ARCH "x86")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
  set(ARCH "x86")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm")
  set(ARCH "arm")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv6")
  set(ARCH "arm")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7-a")
  set(ARCH "arm")
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  set(ARCH "aarch64")
else()
  message(FATAL_ERROR "Unknown processor:" ${CMAKE_SYSTEM_PROCESSOR})
endif()

if (ANDROID AND ${ARCH} STREQUAL "arm")
  # The Android-NDK CMake files somehow fail to set the -march flag for
  # assembly files. Without this flag, the compiler believes that it's
  # building for ARMv5.
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=${CMAKE_SYSTEM_PROCESSOR}")
endif()

if (${ARCH} STREQUAL "x86" AND APPLE)
  # With CMake 2.8.x, ${CMAKE_SYSTEM_PROCESSOR} evalutes to i386 on OS X,
  # but clang defaults to 64-bit builds on OS X unless otherwise told.
  # Set ARCH to x86_64 so clang and CMake agree. This is fixed in CMake 3.
  set(ARCH "x86_64")
endif()

if (OPENSSL_NO_ASM)
  add_definitions(-DOPENSSL_NO_ASM)
  set(ARCH "generic")
endif()

# Declare a dummy target to build all unit tests. Test targets should inject
# themselves as dependencies next to the target definition.
add_custom_target(all_tests)

add_subdirectory(crypto)
add_subdirectory(ssl)
add_subdirectory(ssl/test)
add_subdirectory(tool)
add_subdirectory(decrepit)

if(FUZZ)
  add_subdirectory(fuzz)
endif()

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.2")
  # USES_TERMINAL is only available in CMake 3.2 or later.
  set(MAYBE_USES_TERMINAL USES_TERMINAL)
endif()

add_custom_target(
    run_tests
    COMMAND ${GO_EXECUTABLE} run util/all_tests.go -build-dir
            ${CMAKE_BINARY_DIR}
    COMMAND cd ssl/test/runner &&
            ${GO_EXECUTABLE} test -shim-path $<TARGET_FILE:bssl_shim>
              ${RUNNER_ARGS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS all_tests bssl_shim
    ${MAYBE_USES_TERMINAL})
