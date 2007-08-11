/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __HIWebView__
#define __HIWebView__

#ifndef __LP64__

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
 *    Mac OS X:         in version 10.2.7 and later [32-bit only]
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
 *    Mac OS X:         in version 10.4 and later [32-bit only]
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
 *    Mac OS X:         in version 10.2.7 and later [32-bit only]
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern WebView *
HIWebViewGetWebView(HIViewRef inView);

#endif

#ifdef __cplusplus
}
#endif

#endif
#endif /* __HIWebView__ */
