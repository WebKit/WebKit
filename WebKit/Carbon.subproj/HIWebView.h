#ifndef __HIWebFrameView__
#define __HIWebFrameView__

#ifndef __HIWEBCONTROLLER__
#include <WebKit/WebView.h>
#endif



#include <AvailabilityMacros.h>

#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  HIWebViewCreate()
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
HIWebViewCreate(HIViewRef * outControl);


/*
 *  HIWebViewGetNSView()
 *  
 *  Summary:
 *    Returns the WebKit WebFrameView for a given HIWebFrameView.
 *  
 *  Parameters:
 *    
 *    inView:
 *      The view to inspect.
 *  
 *  Result:
 *    A pointer to aweb frame view object, or NULL.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.2 and later
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern WebView*
HIWebViewGetNSView( HIViewRef inView );

#ifdef __cplusplus
}
#endif

#endif /* __HIWebFrameView__ */

