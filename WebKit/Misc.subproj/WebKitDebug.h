/*	WebKitDebug.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

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

#define WEBKIT_LOG_EVENTS			0x00010000
#define WEBKIT_LOG_VIEW				0x00020000

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
void WebKitDebug(const char *format, ...);
void WebKitDebugAtLevel(unsigned int level, const char *format, ...);
void WebKitLog(NSString *format, ...);
void WebKitLogAtLevel(unsigned int level, NSString *format, ...);


#define _logNeverImplemented() \
   WEBKITDEBUGLEVEL(WEBKIT_LOG_NEVER_IMPLEMENTED, "ERROR (NOT IMPLEMENTED)\n")

#define _logPartiallyImplemented() \
   WEBKITDEBUGLEVEL(WEBKIT_LOG_PARTIALLY_IMPLEMENTED, "WARNING (PARTIALLY IMPLEMENTED)\n")

#define _logNotYetImplemented() \
   WEBKITDEBUGLEVEL (WEBKIT_LOG_NOT_YET_IMPLEMENTED, "WARNING (NOT YET IMPLEMENTED)\n")


#define WEBKITDEBUG(format) WEBKITDEBUGLEVEL(WEBKIT_LOG_GENERIC_DEBUG,format)
#define WEBKITDEBUG1(format,arg1) WEBKITDEBUGLEVEL1(WEBKIT_LOG_GENERIC_DEBUG,format,arg1)
#define WEBKITDEBUG2(format,arg1,arg2) WEBKITDEBUGLEVEL2(WEBKIT_LOG_GENERIC_DEBUG,format,arg1,arg2)
#define WEBKITDEBUG3(format,arg1,arg2,arg3) WEBKITDEBUGLEVEL3(WEBKIT_LOG_GENERIC_DEBUG,format,arg1,arg2,arg3)
#define WEBKITDEBUG4(format,arg1,arg2,arg3,arg4) WEBKITDEBUGLEVEL4(WEBKIT_LOG_GENERIC_DEBUG,format,arg1,arg2,arg3,arg4)
#define WEBKITDEBUG5(format,arg1,arg2,arg3,arg4,arg5) WEBKITDEBUGLEVEL5(WEBKIT_LOG_GENERIC_DEBUG,format,arg1,arg2,arg3,arg4,arg5)
#define WEBKITDEBUG6(format,arg1,arg2,arg3,arg4,arg5,arg6) WEBKITDEBUGLEVEL6(WEBKIT_LOG_GENERIC_DEBUG,format,arg1,arg2,arg3,arg4,arg5,arg6)

#define WEBKITDEBUGLEVEL(level,format) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__, format);\
   WebKitDebugAtLevel (level,format);
            
#define WEBKITDEBUGLEVEL1(level,format,arg1) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   WebKitDebugAtLevel (level,format,arg1);

#if NDEBUG
#define WEBKITDEBUGLEVEL2(level,format,arg1,arg2)
#else
#define WEBKITDEBUGLEVEL2(level,format,arg1,arg2) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   WebKitDebugAtLevel (level,format,arg1,arg2);
#endif

#define WEBKITDEBUGLEVEL3(level,format,arg1,arg2,arg3) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   WebKitDebugAtLevel (level,format,arg1,arg2,arg3);
            
#define WEBKITDEBUGLEVEL4(level,format,arg1,arg2,arg3,arg4) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   WebKitDebugAtLevel (level,format,arg1,arg2,arg3,arg4);
            
#define WEBKITDEBUGLEVEL5(level,format,arg1,arg2,arg3,arg4,arg5) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   WebKitDebugAtLevel (level,format,arg1,arg2,arg3,arg4,arg5);
            
#define WEBKITDEBUGLEVEL6(level,format,arg1,arg2,arg3,arg4,arg5,arg6) \
   WebKitDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   WebKitDebugAtLevel (level,format,arg1,arg2,arg3,arg4,arg5,arg6);
            
#define DEBUG_OBJECT(object) [[object description] cString]

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
    
#ifdef __cplusplus
}
#endif
