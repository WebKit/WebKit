/*
    HIWebView.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.
    
    Public header file.
*/

#ifndef __HIWebView__
#define __HIWebView__

#include <WebKit/WebView.h>

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
 *    Mac OS X:         in version 10.2.7 and later
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
HIWebViewCreate(HIViewRef * outControl);


/*
 *  HIWebViewGetWebView()
 *  
 *  Summary:
 *    Returns the WebKit WebView for a given HIWebView.
 *  
 *  Parameters:
 *    
 *    inView:
 *      The view to inspect.
 *  
 *  Result:
 *    A pointer to a web view object, or NULL.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.2.7 and later
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern WebView*
HIWebViewGetWebView( HIViewRef inView );

#ifdef __cplusplus
}
#endif

#endif /* __HIWebView__ */

