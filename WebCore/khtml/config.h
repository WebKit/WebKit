#if WIN32
#define USE_SYSTEM_MALLOC 1
#endif

#ifdef __cplusplus
#undef new
#undef delete
#include <kxmlcore/FastMalloc.h>
#endif

#if WIN32
// FIXME: Should probably just dump these eventually, but they're needed for now.
typedef unsigned uint;
typedef unsigned short ushort;

#include <assert.h>

#endif
