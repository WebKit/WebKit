#ifndef _FOLLY_CONFIG_H
#define _FOLLY_CONFIG_H 1
 
/* folly-config.h. Generated automatically at end of configure. */
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to "final" if the compiler supports C++11 "final" */
#ifndef FOLLY_FINAL
#define FOLLY_FINAL final
#endif

/* Define to gflags namespace (usually "google" or "gflags") */
#ifndef FOLLY_GFLAGS_NAMESPACE
#define FOLLY_GFLAGS_NAMESPACE gflags
#endif

/* Define to 1 if you have the <bits/c++config.h> header file. */
/* #undef HAVE_BITS_C__CONFIG_H */

/* Define to 1 if you have the <bits/functexcept.h> header file. */
/* #undef HAVE_BITS_FUNCTEXCEPT_H */

/* define if the Boost library is available */
#ifndef FOLLY_HAVE_BOOST
#define FOLLY_HAVE_BOOST /**/
#endif

/* define if the Boost::Chrono library is available */
#ifndef FOLLY_HAVE_BOOST_CHRONO
#define FOLLY_HAVE_BOOST_CHRONO /**/
#endif

/* define if the Boost::Context library is available */
#ifndef FOLLY_HAVE_BOOST_CONTEXT
#define FOLLY_HAVE_BOOST_CONTEXT /**/
#endif

/* define if the Boost::Filesystem library is available */
#ifndef FOLLY_HAVE_BOOST_FILESYSTEM
#define FOLLY_HAVE_BOOST_FILESYSTEM /**/
#endif

/* define if the Boost::PROGRAM_OPTIONS library is available */
#ifndef FOLLY_HAVE_BOOST_PROGRAM_OPTIONS
#define FOLLY_HAVE_BOOST_PROGRAM_OPTIONS /**/
#endif

/* define if the Boost::Regex library is available */
#ifndef FOLLY_HAVE_BOOST_REGEX
#define FOLLY_HAVE_BOOST_REGEX /**/
#endif

/* define if the Boost::System library is available */
#ifndef FOLLY_HAVE_BOOST_SYSTEM
#define FOLLY_HAVE_BOOST_SYSTEM /**/
#endif

/* define if the Boost::Thread library is available */
#ifndef FOLLY_HAVE_BOOST_THREAD
#define FOLLY_HAVE_BOOST_THREAD /**/
#endif

/* Define to 1 if we support clock_gettime(2). */
#ifndef FOLLY_HAVE_CLOCK_GETTIME
#define FOLLY_HAVE_CLOCK_GETTIME 1
#endif

/* Define to 1 if we have cplus_demangle_v3_callback. */
/* #undef HAVE_CPLUS_DEMANGLE_V3_CALLBACK */

/* Define to 1 if you have the <dlfcn.h> header file. */
#ifndef FOLLY_HAVE_DLFCN_H
#define FOLLY_HAVE_DLFCN_H 1
#endif

/* Define to 1 if you have the <dwarf.h> header file. */
/* #undef HAVE_DWARF_H */

/* Define to 1 if you have the <features.h> header file. */
/* #undef HAVE_FEATURES_H */

/* Define to 1 if the compiler supports ifunc */
/* #undef HAVE_IFUNC */

/* Define if we have __int128 */
#ifndef FOLLY_HAVE_INT128_T
#define FOLLY_HAVE_INT128_T 1
#endif

/* Define to 1 if you have the <inttypes.h> header file. */
#ifndef FOLLY_HAVE_INTTYPES_H
#define FOLLY_HAVE_INTTYPES_H 1
#endif

/* Define to 1 if you have the `atomic' library (-latomic). */
/* #undef HAVE_LIBATOMIC */

/* Define to 1 if you have the `bz2' library (-lbz2). */
#ifndef FOLLY_HAVE_LIBBZ2
#define FOLLY_HAVE_LIBBZ2 1
#endif

/* Define to 1 if you have the `dl' library (-ldl). */
/* #undef HAVE_LIBDL */

/* Define to 1 if you have the `double-conversion' library
   (-ldouble-conversion). */
#ifndef FOLLY_HAVE_LIBDOUBLE_CONVERSION
#define FOLLY_HAVE_LIBDOUBLE_CONVERSION 1
#endif

/* Define to 1 if you have the <libdwarf/dwarf.h> header file. */
/* #undef HAVE_LIBDWARF_DWARF_H */

/* Define to 1 if you have the `event' library (-levent). */
#ifndef FOLLY_HAVE_LIBEVENT
#define FOLLY_HAVE_LIBEVENT 1
#endif

/* Define to 1 if you have the `gflags' library (-lgflags). */
#ifndef FOLLY_HAVE_LIBGFLAGS
#define FOLLY_HAVE_LIBGFLAGS 1
#endif

/* Define to 1 if you have the `glog' library (-lglog). */
#ifndef FOLLY_HAVE_LIBGLOG
#define FOLLY_HAVE_LIBGLOG 1
#endif

/* Define to 1 if you have the `lz4' library (-llz4). */
#ifndef FOLLY_HAVE_LIBLZ4
#define FOLLY_HAVE_LIBLZ4 1
#endif

/* Define to 1 if you have the `lzma' library (-llzma). */
#ifndef FOLLY_HAVE_LIBLZMA
#define FOLLY_HAVE_LIBLZMA 1
#endif

/* Define to 1 if you have the `snappy' library (-lsnappy). */
#ifndef FOLLY_HAVE_LIBSNAPPY
#define FOLLY_HAVE_LIBSNAPPY 1
#endif

/* Define to 1 if you have the `z' library (-lz). */
#ifndef FOLLY_HAVE_LIBZ
#define FOLLY_HAVE_LIBZ 1
#endif

/* Define to 1 if you have the `zstd' library (-lzstd). */
/* #undef HAVE_LIBZSTD */

/* Define to 1 if membarrier.h is available */
/* #undef HAVE_LINUX_MEMBARRIER_H */

/* Define to 1 if liblinux-vdso is available */
/* #undef HAVE_LINUX_VDSO */

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the `malloc_size' function. */
#ifndef FOLLY_HAVE_MALLOC_SIZE
#define FOLLY_HAVE_MALLOC_SIZE 1
#endif

/* Define to 1 if you have the `malloc_usable_size' function. */
/* #undef HAVE_MALLOC_USABLE_SIZE */

/* Define to 1 if you have the <memory.h> header file. */
#ifndef FOLLY_HAVE_MEMORY_H
#define FOLLY_HAVE_MEMORY_H 1
#endif

/* Define to 1 if you have the `memrchr' function. */
/* #undef HAVE_MEMRCHR */

/* Define to 1 if you have the `pipe2' function. */
/* #undef HAVE_PIPE2 */

/* Define to 1 if you have the `preadv' function. */
/* #undef HAVE_PREADV */

/* Define to 1 if pthread is avaliable */
#ifndef FOLLY_HAVE_PTHREAD
#define FOLLY_HAVE_PTHREAD 1
#endif

/* Define to 1 if the compiler supports pthread_atfork */
#ifndef FOLLY_HAVE_PTHREAD_ATFORK
#define FOLLY_HAVE_PTHREAD_ATFORK 1
#endif

/* Define to 1 if the system has the type `pthread_spinlock_t'. */
/* #undef HAVE_PTHREAD_SPINLOCK_T */

/* Define to 1 if you have the `pwritev' function. */
/* #undef HAVE_PWRITEV */

/* Define if both -Wshadow-local and -Wshadow-compatible-local are supported.
   */
#ifndef FOLLY_HAVE_SHADOW_LOCAL_WARNINGS
#define FOLLY_HAVE_SHADOW_LOCAL_WARNINGS 1
#endif

/* Define if g++ supports C++1y features. */
#ifndef FOLLY_HAVE_STDCXX_1Y
#define FOLLY_HAVE_STDCXX_1Y /**/
#endif

/* Define to 1 if you have the <stdint.h> header file. */
#ifndef FOLLY_HAVE_STDINT_H
#define FOLLY_HAVE_STDINT_H 1
#endif

/* Define to 1 if you have the <stdlib.h> header file. */
#ifndef FOLLY_HAVE_STDLIB_H
#define FOLLY_HAVE_STDLIB_H 1
#endif

/* Define to 1 if we have a usable std::is_trivially_copyable<T>
   implementation. */
#ifndef FOLLY_HAVE_STD__IS_TRIVIALLY_COPYABLE
#define FOLLY_HAVE_STD__IS_TRIVIALLY_COPYABLE 1
#endif

/* Define to 1 if you have the <strings.h> header file. */
#ifndef FOLLY_HAVE_STRINGS_H
#define FOLLY_HAVE_STRINGS_H 1
#endif

/* Define to 1 if you have the <string.h> header file. */
#ifndef FOLLY_HAVE_STRING_H
#define FOLLY_HAVE_STRING_H 1
#endif

/* Define to 1 if you have the <sys/stat.h> header file. */
#ifndef FOLLY_HAVE_SYS_STAT_H
#define FOLLY_HAVE_SYS_STAT_H 1
#endif

/* Define to 1 if you have the <sys/types.h> header file. */
#ifndef FOLLY_HAVE_SYS_TYPES_H
#define FOLLY_HAVE_SYS_TYPES_H 1
#endif

/* Define to 1 if the architecture allows unaligned accesses */
#ifndef FOLLY_HAVE_UNALIGNED_ACCESS
#define FOLLY_HAVE_UNALIGNED_ACCESS 1
#endif

/* Define to 1 if you have the <unistd.h> header file. */
#ifndef FOLLY_HAVE_UNISTD_H
#define FOLLY_HAVE_UNISTD_H 1
#endif

/* Define to 1 if the compiler has VLA (variable-length array) support,
   otherwise define to 0 */
#ifndef FOLLY_HAVE_VLA
#define FOLLY_HAVE_VLA 1
#endif

/* Define to 1 if the vsnprintf supports returning errors on bad format
   strings. */
/* #undef HAVE_VSNPRINTF_ERRORS */

/* Define to 1 if the libc supports wchar well */
#ifndef FOLLY_HAVE_WCHAR_SUPPORT
#define FOLLY_HAVE_WCHAR_SUPPORT 1
#endif

/* Define to 1 if the linker supports weak symbols. */
/* #undef HAVE_WEAK_SYMBOLS */

/* Define to 1 if the runtime supports XSI-style strerror_r */
#ifndef FOLLY_HAVE_XSI_STRERROR_R
#define FOLLY_HAVE_XSI_STRERROR_R 1
#endif

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#ifndef FOLLY_LT_OBJDIR
#define FOLLY_LT_OBJDIR ".libs/"
#endif

/* Define to 1 for compiler guards for mobile targets. */
/* #undef MOBILE */

/* Define to "override" if the compiler supports C++11 "override" */
#ifndef FOLLY_OVERRIDE
#define FOLLY_OVERRIDE override
#endif

/* Name of package */
#ifndef FOLLY_PACKAGE
#define FOLLY_PACKAGE "folly"
#endif

/* Define to the address where bug reports for this package should be sent. */
#ifndef FOLLY_PACKAGE_BUGREPORT
#define FOLLY_PACKAGE_BUGREPORT "folly@fb.com"
#endif

/* Define to the full name of this package. */
#ifndef FOLLY_PACKAGE_NAME
#define FOLLY_PACKAGE_NAME "folly"
#endif

/* Define to the full name and version of this package. */
#ifndef FOLLY_PACKAGE_STRING
#define FOLLY_PACKAGE_STRING "folly 57.0"
#endif

/* Define to the one symbol short name of this package. */
#ifndef FOLLY_PACKAGE_TARNAME
#define FOLLY_PACKAGE_TARNAME "folly"
#endif

/* Define to the home page for this package. */
#ifndef FOLLY_PACKAGE_URL
#define FOLLY_PACKAGE_URL ""
#endif

/* Define to the version of this package. */
#ifndef FOLLY_PACKAGE_VERSION
#define FOLLY_PACKAGE_VERSION "57.0"
#endif

/* Define to 1 if you have the ANSI C header files. */
#ifndef FOLLY_STDC_HEADERS
#define FOLLY_STDC_HEADERS 1
#endif

/* Define if we need the standard integer traits defined for the type
   `__int128'. */
/* #undef SUPPLY_MISSING_INT128_TRAITS */

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#ifndef FOLLY_TIME_WITH_SYS_TIME
#define FOLLY_TIME_WITH_SYS_TIME 1
#endif

/* Define to 1 if the gflags namespace is not "gflags" */
/* #undef UNUSUAL_GFLAGS_NAMESPACE */

/* Enable jemalloc */
/* #undef USE_JEMALLOC */

/* Define to 1 if we are using libc++. */
#ifndef FOLLY_USE_LIBCPP
#define FOLLY_USE_LIBCPP 1
#endif

/* Version number of package */
#ifndef FOLLY_VERSION
#define FOLLY_VERSION "57.0"
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */
 
/* once: _FOLLY_CONFIG_H */
#endif
