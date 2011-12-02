#ifndef DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#define DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#endif

#include <wtf/Platform.h>

#if PLATFORM(CHROMIUM)
#if !defined(WTF_USE_V8)
#define WTF_USE_V8 1
#endif
#endif /* PLATFORM(CHROMIUM) */

#if !defined(WTF_USE_V8)
#define WTF_USE_V8 0
#endif /* !defined(WTF_USE_V8) */

/* Using V8 implies not using JSC and vice versa */
#define WTF_USE_JSC !WTF_USE_V8

#define JS_EXPORTDATA
#define JS_EXPORTCLASS
#define JS_EXPORT_PRIVATE
#define WTF_EXPORT_PRIVATE

