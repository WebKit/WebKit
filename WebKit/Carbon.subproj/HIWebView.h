/*
    HIWebView.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.
    
    Public header file.
*/

#ifndef __HIWebView__
#define __HIWebView__

#include <Carbon/Carbon.h>

#include <AvailabilityMacros.h>

#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __OBJC__
@class WebView;
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

#ifdef __OBJC__

/*
 *  HIWebViewCreateWithClass(HIViewRef * outControl, Class aClass)
 *  
 *  Summary:
 *    Creates a new web view using the specified subclass of WebView.
 *  
 *  Parameters:
 *    
 *    aClass:
 *      Either WebView, or a subclass, to be created and wrapped in an HIWebView.
 *    outControl:
 *      The new web view.
 *  
 *  Result:
 *    An operating system status code.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.4 and later
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus
HIWebViewCreateWithClass(
  Class       aClass,
  HIViewRef * outControl);

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
extern WebView *
HIWebViewGetWebView(HIViewRef inView);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __HIWebView__ */
