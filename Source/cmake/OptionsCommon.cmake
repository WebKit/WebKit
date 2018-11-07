add_definitions(-DBUILDING_WITH_CMAKE=1)
add_definitions(-DHAVE_CONFIG_H=1)

option(USE_THIN_ARCHIVES "Produce all static libraries as thin archives" ON)
if (USE_THIN_ARCHIVES)
    execute_process(COMMAND ${CMAKE_AR} -V OUTPUT_VARIABLE AR_VERSION)
    if ("${AR_VERSION}" MATCHES "^GNU ar")
        set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> crT <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> crT <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> rT <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> rT <TARGET> <LINK_FLAGS> <OBJECTS>")
    endif ()
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
define_property(TARGET PROPERTY FOLDER INHERITED BRIEF_DOCS "folder" FULL_DOCS "IDE folder name")

# Detect Cortex-A53 core if CPU is ARM64 and OS is Linux.
# Query /proc/cpuinfo for each available core and check reported CPU part number: 0xd03 signals Cortex-A53.
# (see Main ID Register in ARM Cortex-A53 MPCore Processor Technical Reference Manual)
set(WTF_CPU_ARM64_CORTEXA53_INITIALVALUE OFF)
if (WTF_CPU_ARM64 AND (${CMAKE_SYSTEM_NAME} STREQUAL "Linux"))
    execute_process(COMMAND nproc OUTPUT_VARIABLE PROC_COUNT)
    math(EXPR PROC_MAX ${PROC_COUNT}-1)
    foreach (PROC_ID RANGE ${PROC_MAX})
        execute_process(COMMAND taskset -c ${PROC_ID} grep "^CPU part" /proc/cpuinfo OUTPUT_VARIABLE PROC_PART)
        if (PROC_PART MATCHES "0xd03")
            set(WTF_CPU_ARM64_CORTEXA53_INITIALVALUE ON)
            break ()
        endif ()
    endforeach ()
endif ()
option(WTF_CPU_ARM64_CORTEXA53 "Enable Cortex-A53-specific code paths" ${WTF_CPU_ARM64_CORTEXA53_INITIALVALUE})

if (WTF_CPU_ARM64_CORTEXA53)
    if (NOT WTF_CPU_ARM64)
        message(FATAL_ERROR "WTF_CPU_ARM64_CORTEXA53 set without WTF_CPU_ARM64")
    endif ()
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-mfix-cortex-a53-835769)
endif ()

EXPOSE_VARIABLE_TO_BUILD(WTF_CPU_ARM64_CORTEXA53)

set(ARM_TRADITIONAL_DETECTED FALSE)
if (WTF_CPU_ARM)
    set(ARM_THUMB2_TEST_SOURCE
    "
    #if !defined(thumb2) && !defined(__thumb2__)
    #error \"Thumb2 instruction set isn't available\"
    #endif
    int main() {}
   ")

    CHECK_CXX_SOURCE_COMPILES("${ARM_THUMB2_TEST_SOURCE}" ARM_THUMB2_DETECTED)
    if (NOT ARM_THUMB2_DETECTED)
        set(ARM_TRADITIONAL_DETECTED TRUE)
        # See https://bugs.webkit.org/show_bug.cgi?id=159880#c4 for details.
        message(STATUS "Disabling GNU gold linker, because it doesn't support ARM instruction set properly.")
    endif ()
endif ()

# Use ld.gold if it is available and isn't disabled explicitly
CMAKE_DEPENDENT_OPTION(USE_LD_GOLD "Use GNU gold linker" ON
                       "NOT CXX_ACCEPTS_MFIX_CORTEX_A53_835769;NOT ARM_TRADITIONAL_DETECTED;NOT WIN32;NOT APPLE" OFF)
if (USE_LD_GOLD)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
    if ("${LD_VERSION}" MATCHES "GNU gold")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
    else ()
        message(WARNING "GNU gold linker isn't available, using the default system linker.")
        set(USE_LD_GOLD OFF)
    endif ()
endif ()

set(ENABLE_DEBUG_FISSION_DEFAULT OFF)
if (USE_LD_GOLD AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    check_cxx_compiler_flag(-gsplit-dwarf CXX_COMPILER_SUPPORTS_GSPLIT_DWARF)
    if (CXX_COMPILER_SUPPORTS_GSPLIT_DWARF)
        set(ENABLE_DEBUG_FISSION_DEFAULT ON)
    endif ()
endif ()

option(DEBUG_FISSION "Use Debug Fission support" ${ENABLE_DEBUG_FISSION_DEFAULT})

if (DEBUG_FISSION)
    if (NOT USE_LD_GOLD)
        message(FATAL_ERROR "Need GNU gold linker for Debug Fission support")
    endif ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gsplit-dwarf")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsplit-dwarf")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index")
endif ()

# Enable the usage of OpenMP.
#  - At this moment, OpenMP is only used as an alternative implementation
#    to native threads for the parallelization of the SVG filters.
if (USE_OPENMP)
    find_package(OpenMP REQUIRED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif ()

# GTK and WPE use the GNU installation directories as defaults.
if (NOT PORT STREQUAL "GTK" AND NOT PORT STREQUAL "WPE")
    set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Absolute path to library installation directory")
    set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Absolute path to executable installation directory")
    set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Absolute path to install executables executed by the library")
endif ()

# Check whether features.h header exists.
# Including glibc's one defines __GLIBC__, that is used in Platform.h
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_FEATURES_H features.h)

# Check for headers
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_ERRNO_H errno.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_LANGINFO_H langinfo.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_MMAP sys/mman.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_PTHREAD_NP_H pthread_np.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_STRINGS_H strings.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_SYS_PARAM_H sys/param.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_SYS_TIME_H sys/time.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_SYS_TIMEB_H sys/timeb.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_LINUX_MEMFD_H linux/memfd.h)

# Check for functions
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_ALIGNED_MALLOC _aligned_malloc)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_ISDEBUGGERPRESENT IsDebuggerPresent)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_LOCALTIME_R localtime_r)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_MALLOC_TRIM malloc_trim)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_STRNSTR strnstr)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_TIMEGM timegm)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_VASPRINTF vasprintf)

# Check for symbols
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_REGEX_H regexec regex.h)
if (NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"))
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_PTHREAD_MAIN_NP pthread_main_np pthread_np.h)
endif ()
# Windows has signal.h but is missing symbols that are used in calls to signal.
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_SIGNAL_H SIGTRAP signal.h)

# Check for struct members
WEBKIT_CHECK_HAVE_STRUCT(HAVE_STAT_BIRTHTIME "struct stat" st_birthtime sys/stat.h)
WEBKIT_CHECK_HAVE_STRUCT(HAVE_TM_GMTOFF "struct tm" tm_gmtoff time.h)
WEBKIT_CHECK_HAVE_STRUCT(HAVE_TM_ZONE "struct tm" tm_zone time.h)

# Check for int types
check_type_size("__int128_t" INT128_VALUE)

if (HAVE_INT128_VALUE)
  SET_AND_EXPOSE_TO_BUILD(HAVE_INT128_T INT128_VALUE)
endif ()
