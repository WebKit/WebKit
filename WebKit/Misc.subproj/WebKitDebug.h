/*	WebKitDebug.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#ifdef xNDEBUG

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
#define WEBKIT_LOG_DOCUMENTLOAD			0x00001000

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
    __attribute__((__format__ (__printf__, 5, 6)));
    
#ifdef __cplusplus
}
#endif

#define DEBUG_OBJECT(object) [[object description] lossyCString]

#define WEBKITDEBUGLEVEL(level, format...) \
    WebKitLog(level, __FILE__, __LINE__, __PRETTY_FUNCTION__, format)

/*-----------------------------------------------------------------------------
 * Assertion macros
 */

#define WEBKIT_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            NSString *reason = [NSString stringWithFormat:@"assertion failed(%s:%d %s): '%s'", __FILE__, __LINE__, __FUNCTION__, #expr]; \
            [[NSException exceptionWithName:NSGenericException reason:reason userInfo: nil] raise]; \
        } \
    } while (0)

#define WEBKIT_ASSERT_VALID_ARG(arg,expr) \
    do { \
        if (!(expr)) { \
            NSString *reason = [NSString stringWithFormat:@"(%s:%d %s): '%s' fails check: '%s'", __FILE__, __LINE__, __FUNCTION__, #arg, #expr]; \
            [[NSException exceptionWithName:NSInvalidArgumentException reason:reason userInfo: nil] raise]; \
        } \
    } while (0)
    
#define WEBKIT_ASSERT_NOT_NIL(arg) \
    do { \
        if ((arg) == nil) { \
            NSString *reason = [NSString stringWithFormat:@"(%s:%d %s): '%s' is nil", __FILE__, __LINE__, __FUNCTION__, #arg]; \
            [[NSException exceptionWithName:NSInvalidArgumentException reason:reason userInfo: nil] raise]; \
        } \
    } while (0)

#endif

#define _logNeverImplemented() \
   WEBKITDEBUGLEVEL(WEBKIT_LOG_NEVER_IMPLEMENTED, "ERROR (NOT IMPLEMENTED)\n")

#define _logPartiallyImplemented() \
   WEBKITDEBUGLEVEL(WEBKIT_LOG_PARTIALLY_IMPLEMENTED, "WARNING (PARTIALLY IMPLEMENTED)\n")

#define _logNotYetImplemented() \
   WEBKITDEBUGLEVEL (WEBKIT_LOG_NOT_YET_IMPLEMENTED, "WARNING (NOT YET IMPLEMENTED)\n")

#define WEBKITDEBUG(format...) WEBKITDEBUGLEVEL(WEBKIT_LOG_GENERIC_DEBUG, format)
