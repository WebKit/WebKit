/* library/config.h.  Generated from config.h.in by configure.  */
/* library/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* defines how to throw in the noinline attribute */
#define ATTRIBUTE_NOINLINE __attribute__((noinline))

/* Defined if you can do misaligned loads and stores on floats */
#define CAN_DO_FLOAT_MISALIGNED_LDST 1

/* Defined if you can do misaligned loads and stores */
#define CAN_DO_MISALIGNED_LDST 1

/* Determines the prefix for including TSF files from generated code */
#define CODE_GEN_INCLUDE_PREFIX "tsf/"

/* Define to 1 if you have the `accept' function. */
#define HAVE_ACCEPT 0

/* Defined if we can take addresses of labels */
//#define HAVE_ADDRESS_OF_LABEL 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Defined if we are on a big endian machine */
/* #undef HAVE_BIG_ENDIAN */

/* Define to 1 if you have the `bind' function. */
//#define HAVE_BIND 1

/* Defiend if youv'e got BSD sockets */
//#define HAVE_BSDSOCKS 1

/* Defined if we have __sync_bool_compare_and_swap */
//#define HAVE_BUILTIN_CAS 1

/* Defined if we have __sync_fetch_and_add */
//#define HAVE_BUILTIN_XADD 1

/* Defined if we have libbzip2 */
/* #undef HAVE_BZIP2 */

/* Define to 1 if you have the `connect' function. */
//#define HAVE_CONNECT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
//#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `fork' function. */
//#define HAVE_FORK 1

/* Define to 1 if you have the `getaddrinfo' function. */
//#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `gethostbyname' function. */
//#define HAVE_GETHOSTBYNAME 1

/* Define to 1 if you have the `gethostbyname_r' function. */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define to 1 if you have the `getopt' function. */
#define HAVE_GETOPT 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Determines if we have IEEE floating point */
#define HAVE_IEEE_FLOAT 1

/* Define to 1 if you have the `inet_ntoa' function. */
#define HAVE_INET_NTOA 1

/* Define to 1 if you have the `inet_ntop' function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the `listen' function. */
//#define HAVE_LISTEN 1

/* Defined if we are on a little endian machine */
#define HAVE_LITTLE_ENDIAN 1

/* Define to 1 if you have the `localtime' function. */
#define HAVE_LOCALTIME 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the `longjmp' function. */
//#define HAVE_LONGJMP 1

/* Defined if the MAP_ANON constant is defined. */
//#define HAVE_MAP_ANON 1

/* Defined if the MAP_ANONYMOUS constant is defined. */
//#define HAVE_MAP_ANONYMOUS 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdtemp' function. */
//#define HAVE_MKDTEMP 1

/* Define to 1 if you have the `mmap' function. */
//#define HAVE_MMAP 1

/* Define to 1 if you have the `mprotect' function. */
//#define HAVE_MPROTECT 1

/* Define to 1 if you have the `munmap' function. */
//#define HAVE_MUNMAP 1

/* Define to 1 if you have the `pathconf' function. */
//#define HAVE_PATHCONF 1

/* Define to 1 if you have the `pread' function. */
//#define HAVE_PREAD 1

/* Define to 1 if you have the `preadv' function. */
/* #undef HAVE_PREADV */

/* Defined if we have pthreads */
//#define HAVE_PTHREAD 1

/* Define to 1 if you have the `pwrite' function. */
//#define HAVE_PWRITE 1

/* Define to 1 if you have the `pwritev' function. */
/* #undef HAVE_PWRITEV */

/* Define to 1 if you have the `qsort' function. */
#define HAVE_QSORT 1

/* Define to 1 if you have the `readdir_r' function. */
//#define HAVE_READDIR_R 1

/* Define to 1 if you have the `readv' function. */
//#define HAVE_READV 1

/* Define to 1 if you have the `regcomp' function. */
//#define HAVE_REGCOMP 1

/* Define to 1 if you have the `regerror' function. */
//#define HAVE_REGERROR 1

/* Defined if you've got regex functions */
//#define HAVE_REGEX 1

/* Define to 1 if you have the `regexec' function. */
//#define HAVE_REGEXEC 1

/* Define to 1 if you have the `regfree' function. */
//#define HAVE_REGFREE 1

/* Define to 1 if you have the <sched.h> header file. */
//#define HAVE_SCHED_H 1

/* Define to 1 if you have the `sched_yield' function. */
//#define HAVE_SCHED_YIELD 1

/* Define to 1 if you have the `setjmp' function. */
//#define HAVE_SETJMP 1

/* Define to 1 if you have the `setvbuf' function. */
#define HAVE_SETVBUF 1

/* Define to 1 if you have the `socket' function. */
//#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `stpcpy' function. */
#define HAVE_STPCPY 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strsignal' function. */
//#define HAVE_STRSIGNAL 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
//#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the `tempnam' function. */
//#define HAVE_TEMPNAM 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the `writev' function. */
//#define HAVE_WRITEV 1

/* Defined if we can use X86 inline assembly for fences */
//#define HAVE_X86_INLINE_ASM_FENCES 1

/* Defined if we have ZLib */
//#define HAVE_ZLIB 1

/* Define to 1 if you have the `_longjmp' function. */
//#define HAVE__LONGJMP 1

/* Define to 1 if you have the `_setjmp' function. */
/* #undef HAVE__SETJMP */

/* Defined when you're on a sensible OS, unlike NetBSD. */
#define INTTYPES_WORK_RIGHT 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Tell you the native alignment of your platform */
#define NATIVE_ALIGNMENT 16

/* Name of package */
#define PACKAGE "tsf"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "pizlo@mac.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "tsf"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "tsf 1.1.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "tsf"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.1.0"

/* The size of `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `long double', as computed by sizeof. */
#define SIZEOF_LONG_DOUBLE 16

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `off_t', as computed by sizeof. */
#define SIZEOF_OFF_T 8

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 4

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Determines where to place temporary files */
#define TMP_DIRECTORY "/tmp"

/* Version number of package */
#define VERSION "1.1.0"

/* TSF major version number */
#define VERSION_MAJOR 1

/* TSF minor version number */
#define VERSION_MINOR 1

/* TSF patch version number */
#define VERSION_PATCH 0

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */
