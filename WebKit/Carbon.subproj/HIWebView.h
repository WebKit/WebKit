#ifndef __HIWebFrameView__
#define __HIWebFrameView__

#ifndef __HIWEBCONTROLLER__
#include <WebKit/WebController.h>
#endif



#include <AvailabilityMacros.h>

#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  HIWebFrameViewCreate()
 *  
 *  Summary:
 *    Creates a new web view.
 *  
 *  Parameters:
 *    
 *    outControl:
 *      The new web view.
 *  
 *  Result:
 *    An operating system status code.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.2 and later
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
HIWebFrameViewCreate(HIViewRef * outControl);


/*
 *  HIWebFrameViewGetController()
 *  
 *  Summary:
 *    Returns the web controller for a given web view, or NULL if not
 *    bound to one.
 *  
 *  Parameters:
 *    
 *    inView:
 *      The view to inspect.
 *  
 *  Result:
 *    A web controller, or NULL.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.2 and later
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern WebController* 
HIWebFrameViewGetController(HIViewRef inView);

extern WebController*
WebControllerCreateWithHIView( HIViewRef inView, CFStringRef inName );

#ifdef __cplusplus
}
#endif

#endif /* __HIWebFrameView__ */

