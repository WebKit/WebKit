/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import <WebKitLegacy/WebKitLegacy_Private.h>

// Include below only header files for non-macOS platforms, and that can be
// included from both C and C++ source files. macOS header files should be
// enumerated in WebKitLegacy_macOS_Private.h. Files that should only be
// included from C++ source files should be enumerated in the special "cpp"
// section in WebKitLegacy_iOS.private.modulemap.

#import <WebKitLegacy/AbstractPasteboard.h>
#import <WebKitLegacy/DOMHTMLTextAreaElementPrivate.h>
#import <WebKitLegacy/DOMUIKitExtensions.h>
#import <WebKitLegacy/KeyEventCodesIOS.h>
#import <WebKitLegacy/WAKAppKitStubs.h>
#import <WebKitLegacy/WAKResponder.h>
#import <WebKitLegacy/WAKView.h>
#import <WebKitLegacy/WAKWindow.h>
#import <WebKitLegacy/WKContentObservation.h>
#import <WebKitLegacy/WKGraphics.h>
#import <WebKitLegacy/WKTypes.h>
#import <WebKitLegacy/WebCaretChangeListener.h>
#import <WebKitLegacy/WebCoreThread.h>
#import <WebKitLegacy/WebCoreThreadMessage.h>
#import <WebKitLegacy/WebCoreThreadRun.h>
#import <WebKitLegacy/WebEvent.h>
#import <WebKitLegacy/WebEventRegion.h>
#import <WebKitLegacy/WebFixedPositionContent.h>
#import <WebKitLegacy/WebFrameIOS.h>
#import <WebKitLegacy/WebFrameIPhone.h>
// #import <WebKitLegacy/WebGeolocationCoreLocationProvider.h>  // Handled in modulemap file due to requirement that it only be included from C++ source files.
#import <WebKitLegacy/WebGeolocationPrivate.h>
#import <WebKitLegacy/WebGeolocationProviderIOS.h>
#import <WebKitLegacy/WebItemProviderPasteboard.h>
#import <WebKitLegacy/WebMIMETypeRegistry.h>
#import <WebKitLegacy/WebNSStringExtrasIOS.h>
#import <WebKitLegacy/WebNSStringExtrasIPhone.h>
#import <WebKitLegacy/WebPDFViewIOS.h>
#import <WebKitLegacy/WebPDFViewIPhone.h>
#import <WebKitLegacy/WebPDFViewPlaceholder.h>
#import <WebKitLegacy/WebSelectionRect.h>
#import <WebKitLegacy/WebUIKitDelegate.h>
#import <WebKitLegacy/WebUIKitSupport.h>
#import <WebKitLegacy/WebVisiblePosition.h>
