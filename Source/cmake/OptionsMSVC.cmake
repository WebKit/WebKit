add_compile_options(
    /wd4018 /wd4068 /wd4099 /wd4100 /wd4127 /wd4138 /wd4146 /wd4180 /wd4189
    /wd4201 /wd4206 /wd4244 /wd4251 /wd4267 /wd4275 /wd4288 /wd4291 /wd4305
    /wd4309 /wd4344 /wd4355 /wd4389 /wd4396 /wd4456 /wd4457 /wd4458 /wd4459
    /wd4481 /wd4503 /wd4505 /wd4510 /wd4512 /wd4530 /wd4610 /wd4611 /wd4646
    /wd4702 /wd4706 /wd4722 /wd4800 /wd4819 /wd4951 /wd4952 /wd4996 /wd6011
    /wd6031 /wd6211 /wd6246 /wd6255 /wd6387
)

# Create pdb files for debugging purposes, also for Release builds
add_compile_options(/Zi /GS)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG /OPT:ICF /OPT:REF")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /OPT:ICF /OPT:REF")

# We do not use exceptions
add_definitions(-D_HAS_EXCEPTIONS=0)
add_compile_options(/EHa- /EHc- /EHs- /fp:except-)

# We have some very large object files that have to be linked
add_compile_options(/analyze- /bigobj)

# Use CRT security features
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
if (NOT COMPILER_IS_CLANG_CL)
    add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1)
endif ()

# Specify the source code encoding
add_compile_options(/utf-8 /validate-charset)

# Enable the new lambda processor for better C++ conformance with /std:c++17
if (NOT COMPILER_IS_CLANG_CL)
    add_compile_options(/experimental:newLambdaProcessor)
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:NOREF /OPT:NOICF")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:NOREF /OPT:NOICF")

    # To debug linking time issues, uncomment the following three lines:
    #add_compile_options(/Bv)
    #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /VERBOSE /VERBOSE:INCR /TIME")
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /VERBOSE /VERBOSE:INCR /TIME")
elseif (${CMAKE_BUILD_TYPE} MATCHES "Release")
    add_compile_options(/Oy-)
endif ()

if (NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
    link_directories("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
    add_definitions(/MP)
endif ()
if (NOT ${CMAKE_CXX_FLAGS} STREQUAL "")
    string(REGEX REPLACE "(/EH[a-z]+) " "\\1- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable C++ exceptions
    string(REGEX REPLACE "/EHsc$" "/EHs- /EHc- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable C++ exceptions
    string(REGEX REPLACE "/GR " "/GR- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable RTTI
    # More warnings. /W4 should be specified before -Wno-* options for clang-cl.
    string(REGEX REPLACE "/W3" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
    string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(/W4)
endif ()

if (MSVC_STATIC_RUNTIME)
    message(STATUS "Using multithreaded, static version of the run-time library")
    set(MSVC_RUNTIME_COMPILE_FLAG "/MT")
    set(MSVC_RUNTIME_LINKER_FLAGS "/NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")
else ()
    message(STATUS "Using multithreaded, dynamic version of the run-time library")
    set(MSVC_RUNTIME_COMPILE_FLAG "/MD")
    # No linker flags are required
endif ()

foreach (flag_var
    CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
    CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    # Use the multithreaded static runtime library instead of the default DLL runtime.
    string(REGEX REPLACE "/MD" "${MSVC_RUNTIME_COMPILE_FLAG}" ${flag_var} "${${flag_var}}")

    # No debug runtime, even in debug builds.
    if (NOT DEBUG_SUFFIX)
        string(REGEX REPLACE "${MSVC_RUNTIME_COMPILE_FLAG}d" "${MSVC_RUNTIME_COMPILE_FLAG}" ${flag_var} "${${flag_var}}")
        string(REGEX REPLACE "/D_DEBUG" "" ${flag_var} "${${flag_var}}")
    endif ()
endforeach ()

# Make sure incremental linking is turned off, as it creates unacceptably long link times.
string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS})
set(CMAKE_SHARED_LINKER_FLAGS "${replace_CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
set(CMAKE_EXE_LINKER_FLAGS "${replace_CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")

string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_SHARED_LINKER_FLAGS_DEBUG ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${replace_CMAKE_SHARED_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_EXE_LINKER_FLAGS_DEBUG ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${replace_CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")

string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${replace_CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO")
string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${replace_CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO")

if (COMPILER_IS_CLANG_CL)
    # FIXME: The clang-cl visual studio integration seemed to set
    # this to 1900 explicitly even when building in VS2017 with the
    # newest toolset option, but we want to be versioned to match
    # VS2017.
    add_compile_options(-fmsc-version=1911)

    # FIXME: Building with clang-cl seemed to fail with 128 bit int support
    set(HAVE_INT128_T OFF)
    list(REMOVE_ITEM _WEBKIT_CONFIG_FILE_VARIABLES HAVE_INT128_T)
endif ()
