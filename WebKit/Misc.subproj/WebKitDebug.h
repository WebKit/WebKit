/*	WebKitDebug.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <Foundation/NSStringPrivate.h>

#ifdef NDEBUG

#define WEBKITDEBUGLEVEL(level, format...) ((void)0)
#define WEBKIT_ASSERT(expr) ((void)0)
#define WEBKIT_ASSERT_VALID_ARG(arg, expr) ((void)0)
#define WEBKIT_ASSERT_NOT_NIL(arg) ((void)0)

#else

/*-----------------------------------------------------------------------------
* Logging macros
*
* Keep changes in this file synched with kwqdebug.h in WebCore. (Except kwqdebug.h
* shouldn't mention non-core components.)
*/

#define WEBKIT_LOG_NEVER_IMPLEMENTED		0x00000001
#define WEBKIT_LOG_PARTIALLY_IMPLEMENTED	0x00000002
#define WEBKIT_LOG_NOT_YET_IMPLEMENTED		0x00000004
#define WEBKIT_LOG_GENERIC_DEBUG		0x00000008
#define WEBKIT_LOG_GENERIC_ERROR		0x00000010
#define WEBKIT_LOG_TIMING			0x00000020
#define WEBKIT_LOG_LOADING			0x00000040
#define WEBKIT_LOG_MEMUSAGE			0x00000080
#define WEBKIT_LOG_FONTCACHE			0x00000100
#define WEBKIT_LOG_FONTCACHECHARMISS		0x00000200
#define WEBKIT_LOG_DOWNLOAD			0x00000800
#define WEBKIT_LOG_DOCUMENTLOAD			0x00001000
#define WEBKIT_LOG_PLUGINS			0x00002000

#define WEBKIT_LOG_EVENTS			0x00010000
#define WEBKIT_LOG_VIEW				0x00020000

#define WEBKIT_LOG_REDIRECT			0x00040000

// ranges reserved for different uses to enable easy isolation of certain log messages
#define WEBKIT_LOG_GENERIC_RANGE		0x000000FF
#define WEBKIT_LOG_CORE_RANGE			0x0000FF00	
#define WEBKIT_LOG_KIT_RANGE			0x00FF0000
#define WEBKIT_LOG_CLIENT_RANGE			0xFF000000

#define WEBKIT_LOG_NONE				0
#define WEBKIT_LOG_ALL				0xFFFFFFFF

#ifdef __cplusplus
extern "C" {
#endif

void WebKitSetLogLevel(int mask);
unsigned int WebKitGetLogLevel(void);
void WebKitLog(unsigned int level, const char *file, int line, const char *function, const char *format, ...)
// FIXME: When Radar 2920557 is fixed, we can add this back and turn the -Wmissing-format-attribute
// switch back on. PFE precompiled headers currently prevent this from working.
//    __attribute__((__format__ (__printf__, 5, 6)))
;

#ifdef __cplusplus
}
#endif

#define DEBUG_OBJECT(object) [[[object description] displayableString] lossyCString]

#define WEBKITDEBUGLEVEL(level, format...) \
    WebKitLog(level, __FILE__, __LINE__, __PRETTY_FUNCTION__, format)

/*-----------------------------------------------------------------------------
 * Assertion macros
 */

#import <signal.h>
#import <sys/types.h>
#import <sys/time.h>
#import <sys/resource.h>

#define WEBKIT_ASSERTION_FAILURE \
    do { \
        struct rlimit _rlimit = {RLIM_INFINITY, RLIM_INFINITY}; \
        setrlimit(RLIMIT_CORE, &_rlimit); \
        fprintf(stderr, "assertion failure at %s:%d %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        raise(SIGQUIT); \
    } while (0)

#define WEBKIT_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            WEBKIT_ASSERTION_FAILURE; \
        } \
    } while (0)

#define WEBKIT_ASSERT_VALID_ARG(arg,expr) \
    do { \
        if (!(expr)) { \
            WEBKIT_ASSERTION_FAILURE; \
        } \
    } while (0)
    
#define WEBKIT_ASSERT_NOT_NIL(arg) \
    do { \
        if ((arg) == nil) { \
            WEBKIT_ASSERTION_FAILURE; \
        } \
    } while (0)

#endif

#define _logNeverImplemented() \
   WEBKITDEBUGLEVEL(WEBKIT_LOG_NEVER_IMPLEMENTED, "ERROR (NOT IMPLEMENTED)")

#define _logPartiallyImplemented() \
   WEBKITDEBUGLEVEL(WEBKIT_LOG_PARTIALLY_IMPLEMENTED, "WARNING (PARTIALLY IMPLEMENTED)")

#define _logNotYetImplemented() \
   WEBKITDEBUGLEVEL (WEBKIT_LOG_NOT_YET_IMPLEMENTED, "WARNING (NOT YET IMPLEMENTED)")

#define WEBKITDEBUG(format...) WEBKITDEBUGLEVEL(WEBKIT_LOG_GENERIC_DEBUG, format)

#ifdef IF_MALLOC_TESTING
#ifdef __cplusplus
extern "C" {
void setupDebugMalloc(void);
void clearDebugMalloc(void);
void resetDebugMallocCounters(void);
void printDebugMallocCounters(void);
}
#else
void setupDebugMalloc(void);
void clearDebugMalloc(void);
void resetDebugMallocCounters(void);
void printDebugMallocCounters(void);
#endif
#endif

