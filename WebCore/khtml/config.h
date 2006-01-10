#if !WIN32
#ifdef __cplusplus
#undef new
#undef delete
#include <kxmlcore/FastMalloc.h>
#endif
#endif

#if WIN32
// FIXME: Should probably just dump these eventually, but they're needed for now.
typedef unsigned uint;
typedef unsigned short ushort;

#include <assert.h>
#endif
