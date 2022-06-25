/*
 * Copyright (C) 2005, 2008, 2009 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKitLegacy/WebDOMOperations.h>
#import <JavaScriptCore/JSBase.h>

#if TARGET_OS_IPHONE
#import <WebKitLegacy/WAKAppKitStubs.h>
#else
#import <AppKit/NSEvent.h>
#import <WebKitLegacy/DOMWheelEvent.h>
#endif

@interface DOMElement (WebDOMElementOperationsPrivate)
+ (DOMElement *)_DOMElementFromJSContext:(JSContextRef)context value:(JSValueRef)value;
@end

@interface DOMHTMLInputElement (WebDOMHTMLInputElementOperationsPrivate)
- (BOOL)_isAutofilled;
- (void)_setAutofilled:(BOOL)autofilled;

- (BOOL)_isAutoFilledAndViewable;
- (void)_setAutoFilledAndViewable:(BOOL)autoFilledAndViewable;
@end

@interface DOMNode (WebDOMNodeOperationsPendingPublic)
- (NSString *)markupString;
- (NSRect)_renderRect:(bool *)isReplaced;
@end

typedef BOOL (^WebArchiveSubframeFilter)(WebFrame* subframe);

@interface DOMNode (WebDOMNodeOperationsPrivate)
- (WebArchive *)webArchiveByFilteringSubframes:(WebArchiveSubframeFilter)webArchiveSubframeFilter;
#if TARGET_OS_IPHONE
- (BOOL)isHorizontalWritingMode;
- (void)hidePlaceholder;
- (void)showPlaceholderIfNecessary;
#endif
@end

#if !TARGET_OS_IPHONE
@interface DOMWheelEvent (WebDOMWheelEventOperationsPrivate)
- (NSEventPhase)_phase;
- (NSEventPhase)_momentumPhase;
@end
#endif
