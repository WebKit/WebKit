/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
#ifndef CONFIG_H_
#define CONFIG_H_

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define if you have alloca, as a function or macro.  */
/* #undef HAVE_ALLOCA */

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
/* #undef HAVE_ALLOCA_H */

/* Define if you have a working `mmap' system call.  */
/* #undef HAVE_MMAP */

/* Define if you have <vfork.h>.  */
/* #undef HAVE_VFORK_H */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
/* #undef STACK_DIRECTION */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* The number of bytes in a char.  */
/* #undef SIZEOF_CHAR */

/* The number of bytes in a char *.  */
/* #undef SIZEOF_CHAR_P */

/* The number of bytes in a int.  */
/* #undef SIZEOF_INT */

/* The number of bytes in a long.  */
/* #undef SIZEOF_LONG */

/* Define if you have the _IceTransNoListen function.  */
/* #undef HAVE__ICETRANSNOLISTEN */

/* Define if you have the __argz_count function.  */
/* #undef HAVE___ARGZ_COUNT */

/* Define if you have the __argz_next function.  */
/* #undef HAVE___ARGZ_NEXT */

/* Define if you have the __argz_stringify function.  */
/* #undef HAVE___ARGZ_STRINGIFY */

/* Define if you have the freeaddrinfo function.  */
/* #undef HAVE_FREEADDRINFO */

/* Define if you have the gai_strerror function.  */
/* #undef HAVE_GAI_STRERROR */

/* Define if you have the getcwd function.  */
/* #undef HAVE_GETCWD */

/* Define if you have the getgroups function.  */
/* #undef HAVE_GETGROUPS */

/* Define if you have the gethostbyname2 function.  */
/* #undef HAVE_GETHOSTBYNAME2 */

/* Define if you have the gethostbyname2_r function.  */
/* #undef HAVE_GETHOSTBYNAME2_R */

/* Define if you have the gethostbyname_r function.  */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define if you have the getmntinfo function.  */
/* #undef HAVE_GETMNTINFO */

/* Define if you have the getnameinfo function.  */
/* #undef HAVE_GETNAMEINFO */

/* Define if you have the getpagesize function.  */
/* #undef HAVE_GETPAGESIZE */

/* Define if you have the getpeername function.  */
/* #undef HAVE_GETPEERNAME */

/* Define if you have the getpt function.  */
/* #undef HAVE_GETPT */

/* Define if you have the getsockname function.  */
/* #undef HAVE_GETSOCKNAME */

/* Define if you have the getsockopt function.  */
/* #undef HAVE_GETSOCKOPT */

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the grantpt function.  */
/* #undef HAVE_GRANTPT */

/* Define if you have the inet_ntop function.  */
/* #undef HAVE_INET_NTOP */

/* Define if you have the inet_pton function.  */
/* #undef HAVE_INET_PTON */

/* Define if you have the initgroups function.  */
/* #undef HAVE_INITGROUPS */

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mkstemps function.  */
#define HAVE_MKSTEMPS 1

/* Define if you have the munmap function.  */
/* #undef HAVE_MUNMAP */

/* Define if you have the openpty function.  */
/* #undef HAVE_OPENPTY */

/* Define if you have the ptsname function.  */
/* #undef HAVE_PTSNAME */

/* Define if you have the putenv function.  */
/* #undef HAVE_PUTENV */

/* Define if you have the random function.  */
#define HAVE_RANDOM 1

/* Define if you have the res_init function.  */
/* #undef HAVE_RES_INIT */

/* Define if you have the setegid function.  */
#define HAVE_SETEGID 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setfsent function.  */
/* #undef HAVE_SETFSENT */

/* Define if you have the setgroups function.  */
/* #undef HAVE_SETGROUPS */

/* Define if you have the setlocale function.  */
/* #undef HAVE_SETLOCALE */

/* Define if you have the setmntent function.  */
/* #undef HAVE_SETMNTENT */

/* Define if you have the setpriority function.  */
/* #undef HAVE_SETPRIORITY */

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the stpcpy function.  */
/* #undef HAVE_STPCPY */

/* Define if you have the strcasecmp function.  */
/* #undef HAVE_STRCASECMP */

/* Define if you have the strchr function.  */
/* #undef HAVE_STRCHR */

/* Define if you have the strfmon function.  */
/* #undef HAVE_STRFMON */

/* Define if you have the unlockpt function.  */
/* #undef HAVE_UNLOCKPT */

/* Define if you have the unsetenv function.  */
#define HAVE_UNSETENV 1

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the </usr/src/sys/gnu/i386/isa/sound/awe_voice.h> header file.  */
/* #undef HAVE__USR_SRC_SYS_GNU_I386_ISA_SOUND_AWE_VOICE_H */

/* Define if you have the </usr/src/sys/i386/isa/sound/awe_voice.h> header file.  */
/* #undef HAVE__USR_SRC_SYS_I386_ISA_SOUND_AWE_VOICE_H */

/* Define if you have the <X11/ICE/ICElib.h> header file.  */
/* #undef HAVE_X11_ICE_ICELIB_H */

/* Define if you have the <X11/extensions/XShm.h> header file.  */
/* #undef HAVE_X11_EXTENSIONS_XSHM_H */

/* Define if you have the <X11/extensions/shape.h> header file.  */
/* #undef HAVE_X11_EXTENSIONS_SHAPE_H */

/* Define if you have the <alloca.h> header file.  */
/* #undef HAVE_ALLOCA_H */

/* Define if you have the <ansidecl.h> header file.  */
/* #undef HAVE_ANSIDECL_H */

/* Define if you have the <argz.h> header file.  */
/* #undef HAVE_ARGZ_H */

/* Define if you have the <awe_voice.h> header file.  */
/* #undef HAVE_AWE_VOICE_H */

/* Define if you have the <ctype.h> header file.  */
/* #undef HAVE_CTYPE_H */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <dlfcn.h> header file.  */
/* #undef HAVE_DLFCN_H */

/* Define if you have the <float.h> header file.  */
#define HAVE_FLOAT_H 1

/* Define if you have the <fnmatch.h> header file.  */
#define HAVE_FNMATCH_H 1

/* Define if you have the <fp_class.h> header file.  */
/* #undef HAVE_FP_CLASS_H */

/* Define if you have the <fstab.h> header file.  */
/* #undef HAVE_FSTAB_H */

/* Define if you have the <ieeefp.h> header file.  */
/* #undef HAVE_IEEEFP_H */

/* Define if you have the <libutil.h> header file.  */
/* #undef HAVE_LIBUTIL_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <linux/awe_voice.h> header file.  */
/* #undef HAVE_LINUX_AWE_VOICE_H */

/* Define if you have the <linux/socket.h> header file.  */
/* #undef HAVE_LINUX_SOCKET_H */

/* Define if you have the <locale.h> header file.  */
/* #undef HAVE_LOCALE_H */

/* Define if you have the <machine/soundcard.h> header file.  */
/* #undef HAVE_MACHINE_SOUNDCARD_H */

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <math.h> header file.  */
/* #undef HAVE_MATH_H */

/* Define if you have the <mntent.h> header file.  */
/* #undef HAVE_MNTENT_H */

/* Define if you have the <monetary.h> header file.  */
/* #undef HAVE_MONETARY_H */

/* Define if you have the <nan.h> header file.  */
/* #undef HAVE_NAN_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <netinet/in.h> header file.  */
/* #undef HAVE_NETINET_IN_H */

/* Define if you have the <nl_types.h> header file.  */
/* #undef HAVE_NL_TYPES_H */

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <pthread/linuxthreads/pthread.h> header file.  */
/* #undef HAVE_PTHREAD_LINUXTHREADS_PTHREAD_H */

/* Define if you have the <pty.h> header file.  */
/* #undef HAVE_PTY_H */

/* Define if you have the <sigaction.h> header file.  */
/* #undef HAVE_SIGACTION_H */

/* Define if you have the <socketbits.h> header file.  */
/* #undef HAVE_SOCKETBITS_H */

/* Define if you have the <soundcard.h> header file.  */
/* #undef HAVE_SOUNDCARD_H */

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/acl.h> header file.  */
/* #undef HAVE_SYS_ACL_H */

/* Define if you have the <sys/cdefs.h> header file.  */
#define HAVE_SYS_CDEFS_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/mman.h> header file.  */
#define HAVE_SYS_MMAN_H 1

/* Define if you have the <sys/mnttab.h> header file.  */
/* #undef HAVE_SYS_MNTTAB_H */

/* Define if you have the <sys/mount.h> header file.  */
/* #undef HAVE_SYS_MOUNT_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/soundcard.h> header file.  */
/* #undef HAVE_SYS_SOUNDCARD_H */

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/ucred.h> header file.  */
/* #undef HAVE_SYS_UCRED_H */

/* Define if you have the <sysent.h> header file.  */
/* #undef HAVE_SYSENT_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <util.h> header file.  */
/* #undef HAVE_UTIL_H */

/* Define if you have the <values.h> header file.  */
/* #undef HAVE_VALUES_H */

/* Name of package */
#define PACKAGE "Labyrinth"

/* Version number of package */
#define VERSION "not-versioned"

/* C++ compiler supports template repository */
/* #undef HAVE_TEMPLATE_REPOSITORY */

/* Define if you have stpcpy */
/* #undef HAVE_STPCPY */

/* Define if your locale.h file contains LC_MESSAGES */
/* #undef HAVE_LC_MESSAGES */

/* Define if you have a STL implementation by SGI */
/* #undef HAVE_SGI_STL */

/* Define if you have a STL implementation by HP */
/* #undef HAVE_HP_STL */

/* Defines if your system has the crypt function */
/* #undef HAVE_CRYPT */

/* Define the real type of socklen_t */
/* #undef socklen_t */

/* Compatibility define */
/* #undef ksize_t */

/* Define if you have libz */
/* #undef HAVE_LIBZ */

/* Define if you want Xinerama support */
/* #undef HAVE_XINERAMA */

/* Define if you have libpng */
/* #undef HAVE_LIBPNG */

/* Define if you have libjpeg */
/* #undef HAVE_LIBJPEG */

/* Define if you have libtiff */
/* #undef HAVE_LIBTIFF */

/* Define if you have libtiff */
/* #undef HAVE_LIBTIFF */

/* The prefix to use as fallback */
/* #undef KDEDIR */

/* You _must_ have bool */
/* #undef HAVE_BOOL */

/* Define if you have the usleep function */
/* #undef HAVE_USLEEP */

/* Define if you have getdomainname */
/* #undef HAVE_GETDOMAINNAME */

/* Define if you have getdomainname prototype */
/* #undef HAVE_GETDOMAINNAME_PROTO */

/* Define if you have gethostname prototype */
/* #undef HAVE_GETHOSTNAME_PROTO */

/* Define if you have gethostname */
/* #undef HAVE_GETHOSTNAME */

/* Define if you have random */
#define HAVE_RANDOM 1

/* Define if sys/stat.h declares S_ISSOCK. */
#define HAVE_S_ISSOCK 1

/* Define the file for mount entries */
/* #undef MTAB_FILE */

/* Define if using the dmalloc debugging malloc package */
/* #undef WITH_DMALLOC */

/* Define if you want MIT-SHM support */
/* #undef HAVE_MITSHM */

/* where rgb.txt is in */
/* #undef X11_RGBFILE */

/* what C++ computer were used for compilation */
/* #undef KDE_COMPILER_VERSION */

/* what OS used for compilation */
/* #undef KDE_COMPILING_OS */

/* Distribution Text to append to OS */
/* #undef KDE_DISTRIBUTION_TEXT */

/* Define if you have libaudioIO (required if you want to have libaudioio support) */
/* #undef HAVE_LIBAUDIOIO */

/* Define if you have libaudio (required if you want to have NAS support) */
/* #undef HAVE_LIBAUDIONAS */

/* Define if you have libaudiofile (required for playing wavs with aRts) */
/* #undef HAVE_LIBAUDIOFILE */

/* Define if your system supports realtime scheduling */
/* #undef HAVE_REALTIME_SCHED */

/* Define if you have getdomainname */
/* #undef HAVE_GETDOMAINNAME */

/* Define if you have getdomainname prototype */
/* #undef HAVE_GETDOMAINNAME_PROTO */

/* Define if ioctl is declared as int ioctl(int d, int request,...) */
/* #undef HAVE_IOCTL_INT_INT_DOTS */

/* Define if ioctl is declared as int ioctl(int d, unsigned long request,...) */
/* #undef HAVE_IOCTL_INT_ULONG_DOTS */

/* Define if ioctl is declared as int ioctl(int d, unsigned long int request,...) */
/* #undef HAVE_IOCTL_INT_ULONGINT_DOTS */

/* Define if you want to use optimized x86 float to int conversion code */
/* #undef HAVE_X86_FLOAT_INT */

/* Define if your assembler supports x86 SSE instructions */
/* #undef HAVE_X86_SSE */

/* Define if you want to use glibc facilities to emulate stdio accesses in artsdsp */
/* #undef HAVE_ARTSDSP_STDIO_EMU */

/* Define if you have a working libpthread (will enable threaded code) */
/* #undef HAVE_LIBPTHREAD */

/* Define if you have libasound (required for alsa support) */
/* #undef HAVE_LIBASOUND */

/* Define if netdb.h defines struct addrinfo */
/* #undef HAVE_STRUCT_ADDRINFO */

/* Define if getaddrinfo is present */
/* #undef HAVE_GETADDRINFO */

/* Define if getaddrinfo is broken and should be replaced */
/* #undef HAVE_BROKEN_GETADDRINFO */

/* Define if getaddrinfo returns AF_UNIX sockets */
/* #undef GETADDRINFO_RETURNS_UNIX */

/* Define if getaddrinfo is broken and should be replaced */
/* #undef HAVE_BROKEN_GETADDRINFO */

/* Define if getaddrinfo returns AF_UNIX sockets */
/* #undef GETADDRINFO_RETURNS_UNIX */

/* Define if getaddrinfo returns AF_UNIX sockets */
/* #undef GETADDRINFO_RETURNS_UNIX */

/* Define if getaddrinfo returns AF_UNIX sockets */
/* #undef GETADDRINFO_RETURNS_UNIX */

/* Define if struct sockaddr has member sa_len */
/* #undef HAVE_SOCKADDR_SA_LEN */

/* Define if we have struct sockaddr_in6 in netinet/in.h */
/* #undef HAVE_SOCKADDR_IN6 */

/* Define if this system already has sin6_scope_id in sockaddr_in6 */
/* #undef HAVE_SOCKADDR_IN6_SCOPE_ID */

/* Lookup mode for IPv6 addresses: 0 for always, 1 for check and 2 for disabled */
/* #undef KDE_IPV6_LOOKUP_MODE */

/* Lookup mode for IPv6 addresses: 0 for always, 1 for check and 2 for disabled */
/* #undef KDE_IPV6_LOOKUP_MODE */

/* Lookup mode for IPv6 addresses: 0 for always, 1 for check and 2 for disabled */
/* #undef KDE_IPV6_LOOKUP_MODE */

/* Lookup mode for IPv6 addresses: 0 for always, 1 for check and 2 for disabled */
/* #undef KDE_IPV6_LOOKUP_MODE */

/* Defines if you have CUPS (Common UNIX Printing System) */
/* #undef HAVE_CUPS */

/* path to su */
/* #undef __PATH_SU */

/* Define if you have POSIX.1b scheduling */
/* #undef POSIX1B_SCHEDULING */

/* KDE bindir */
/* #undef __KDE_BINDIR */

/* Define if you have openpty in -lutil */
/* #undef HAVE_OPENPTY */

/* Define if struct ucred is present from sys/socket.h */
/* #undef HAVE_STRUCT_UCRED */

/* Defines the executable of xmllint */
/* #undef XMLLINT */

/* Define if you have isnan */
/* #undef HAVE_ISNAN */

/* Define if you have isinf */
/* #undef HAVE_ISINF */

/* Define if you have pow */
/* #undef HAVE_POW */

/* Define if you have floor */
/* #undef HAVE_FLOOR */

/* Define if you have fabs */
/* #undef HAVE_FABS */

/* Define if the libbz2 functions need the BZ2_ prefix */
/* #undef NEED_BZ2_PREFIX */

/* Defines if bzip2 is compiled */
/* #undef HAVE_BZIP2_SUPPORT */

/* Define if your system has libfam */
/* #undef HAVE_FAM */

/* Define if you have libz */
/* #undef HAVE_LIBZ */

/* Define, to enable volume management (Solaris 2.x), if you have -lvolmgt */
/* #undef HAVE_VOLMGT */

/* Define if you need to use the GNU extensions */
/* #undef _GNU_SOURCE */

/* Define if your system has Linux Directory Notification */
/* #undef HAVE_DNOTIFY */

/* if setgroups() takes short *as second arg */
/* #undef HAVE_SHORTSETGROUPS */

/* Define if you have isinf */
#define HAVE_FUNC_ISINF 1

/* Define if you have finite */
#define HAVE_FUNC_FINITE 1

/* Define if you have _finite */
/* #undef HAVE_FUNC__FINITE */

/* Define if you have isnan */
#define HAVE_FUNC_ISNAN 1

/* Define if you have pcreposix libraries and header files. */
/* #undef HAVE_PCREPOSIX */

/* If we are going to use OpenSSL */
/* #undef HAVE_SSL */

/* Define if you have OpenSSL < 0.9.6 */
/* #undef HAVE_OLD_SSL_API */

/* this is included everywhere anyway (mostly indirecty)
   and we need it for socklen_t being always available */
#include <sys/types.h>

/* provide a definition for a 32 bit entity, usable as a typedef, possibly
   extended by "unsigned" */
/* #undef INT32_BASETYPE */
#ifdef SIZEOF_INT
#if SIZEOF_INT == 4
#define INT32_BASETYPE int
#endif
#endif
#if !defined(INT32_BASETYPE) && defined(SIZEOF_LONG)
#if SIZEOF_LONG == 4
#define INT32_BASETYPE long
#endif
#endif
#ifndef INT32_BASETYPE
#define INT32_BASETYPE int
#endif


/*
 * jpeg.h needs HAVE_BOOLEAN, when the system uses boolean in system
 * headers and I'm too lazy to write a configure test as long as only
 * unixware is related
 */
#ifdef _UNIXWARE
#define HAVE_BOOLEAN
#endif

/* random() returns a value between 0 and RANDOM_MAX.
 * RANDOM_MAX is needed to generate good random numbers. (Nicolas)
 */
#ifndef HAVE_RANDOM
#define HAVE_RANDOM
#define RANDOM_MAX 2^31
#ifdef __cplusplus
extern "C"
#endif
long int random(void); /* defined in fakes.c */
#ifdef __cplusplus
extern "C"
#endif
void srandom(unsigned int seed);
#else
/* normal random() */
#define RANDOM_MAX RAND_MAX
#endif

#if !defined(HAVE_MKSTEMPS)
#ifdef __cplusplus
extern "C"
#endif
int mkstemps (char* _template, int suffix_len); /* defined in fakes.c */
#endif

#endif

/* Define if you have the <dirent.h> header file, and it defines `DIR'. */
#define HAVE_DIRENT_H 1

/* Define if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define if you have the <float.h> header file. */
#define HAVE_FLOAT_H 1

/* Define if you have the <fnmatch.h> header file. */
#define HAVE_FNMATCH_H 1

/* Define if you have the `fork' function. */
#define HAVE_FORK 1

/* Define if Foundation source tree exists */
#define HAVE_FOUNDATION_SOURCES 1

/* Define if you have finite */
#define HAVE_FUNC_FINITE 1

/* Define if you have isinf */
#define HAVE_FUNC_ISINF 1

/* Define if you have isnan */
#define HAVE_FUNC_ISNAN 1

/* Define if you have _finite */
/* #undef HAVE_FUNC__FINITE */

/* Define if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Define if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define if you have the `mkstemps' function. */
#define HAVE_MKSTEMPS 1

/* Define if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define if you have the <paths.h> header file. */
#define HAVE_PATHS_H 1

/* Define if you have the `random' function. */
#define HAVE_RANDOM 1

/* Define if you have the `setegid' function. */
#define HAVE_SETEGID 1

/* Define if you have the `setenv' function. */
#define HAVE_SETENV 1

/* Define if you have the `seteuid' function. */
#define HAVE_SETEUID 1

/* Define if you have the <sigaction.h> header file. */
/* #undef HAVE_SIGACTION_H */

/* Define if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define if you have the <socketbits.h> header file. */
/* #undef HAVE_SOCKETBITS_H */

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define if you have the `stpcpy' function. */
/* #undef HAVE_STPCPY */

/* Define if you have the `strfmon' function. */
/* #undef HAVE_STRFMON */

/* Define if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if you have the <sysent.h> header file. */
/* #undef HAVE_SYSENT_H */

/* Define if you have the <sys/cdefs.h> header file. */
#define HAVE_SYS_CDEFS_H 1

/* Define if you have the <sys/dir.h> header file, and it defines `DIR'. */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define if you have the <sys/ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if you have the `unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Name of package */
#define PACKAGE "Labyrinth"

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly. */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "not-versioned"

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
/* #undef inline */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */
