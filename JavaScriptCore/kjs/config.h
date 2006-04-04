#include <kxmlcore/Platform.h>

#if PLATFORM(DARWIN)

#define HAVE_ERRNO_H 1
#define HAVE_FUNC_ISINF 1
#define HAVE_FUNC_ISNAN 1
#define HAVE_MMAP 1
#define HAVE_SBRK 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TIMEB_H 1

#elif PLATFORM(WIN_OS)

#define HAVE_FLOAT_H 1
#define HAVE_FUNC__FINITE 1
#define HAVE_SYS_TIMEB_H 1


#define USE_SYSTEM_MALLOC 1

#else

// FIXME: is this actually used or do other platforms generate their
// own config.h?

#define HAVE_ERRNO_H 1
#define HAVE_FUNC_ISINF 1
#define HAVE_FUNC_ISNAN 1
#define HAVE_MMAP 1
#define HAVE_SBRK 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1

#endif

#define HAVE_PCREPOSIX 1

// FIXME: if all platforms have these, do they really need #defines?
#define HAVE_STDINT_H 1
#define HAVE_STRING_H 1

#define KXC_CHANGES 1

#ifdef __cplusplus
#undef new
#undef delete
#include <kxmlcore/FastMalloc.h>
#endif
