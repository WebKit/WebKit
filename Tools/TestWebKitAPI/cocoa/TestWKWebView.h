/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if WK_API_ENABLED

#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

@class _WKProcessPoolConfiguration;

#if PLATFORM(IOS_FAMILY)
@class _WKActivatedElementInfo;
@protocol UITextInputMultiDocument;
@protocol UITextInputPrivate;
#endif

@interface WKWebView (AdditionalDeclarations)
#if PLATFORM(MAC)
- (void)paste:(id)sender;
- (void)changeAttributes:(id)sender;
- (void)changeColor:(id)sender;
- (void)superscript:(id)sender;
- (void)subscript:(id)sender;
- (void)unscript:(id)sender;
#endif
@end

@interface WKWebView (TestWebKitAPI)
- (BOOL)_synchronouslyExecuteEditCommand:(NSString *)command argument:(NSString *)argument;
@end

@interface TestMessageHandler : NSObject <WKScriptMessageHandler>
- (void)addMessage:(NSString *)message withHandler:(dispatch_block_t)handler;
@end

@interface TestWKWebView : WKWebView
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration processPoolConfiguration:(_WKProcessPoolConfiguration *)processPoolConfiguration;
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration addToWindow:(BOOL)addToWindow;
- (void)clearMessageHandlers:(NSArray *)messageNames;
- (void)performAfterReceivingMessage:(NSString *)message action:(dispatch_block_t)action;
- (void)loadTestPageNamed:(NSString *)pageName;
- (void)synchronouslyLoadHTMLString:(NSString *)html;
- (void)synchronouslyLoadHTMLString:(NSString *)html baseURL:(NSURL *)url;
- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName;
- (id)objectByEvaluatingJavaScript:(NSString *)script;
- (NSString *)stringByEvaluatingJavaScript:(NSString *)script;
- (void)waitForMessage:(NSString *)message;
- (void)performAfterLoading:(dispatch_block_t)actions;
- (void)waitForNextPresentationUpdate;
- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName;
- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName;
- (void)collapseToStart;
- (void)collapseToEnd;
@end

#if PLATFORM(IOS_FAMILY)
@interface TestWKWebView (IOSOnly)
@property (nonatomic, readonly) UIView <UITextInputPrivate, UITextInputMultiDocument> *textInputContentView;
@property (nonatomic, readonly) RetainPtr<NSArray> selectionRectsAfterPresentationUpdate;
@property (nonatomic, readonly) CGRect caretViewRectInContentCoordinates;
@property (nonatomic, readonly) NSArray<NSValue *> *selectionViewRectsInContentCoordinates;
- (_WKActivatedElementInfo *)activatedElementAtPosition:(CGPoint)position;
@end
#endif

#if PLATFORM(MAC)
@interface TestWKWebView (MacOnly)
// Simulates clicking with a pressure-sensitive device, if possible.
- (void)mouseDownAtPoint:(NSPoint)pointInWindow simulatePressure:(BOOL)simulatePressure;
- (void)mouseDragToPoint:(NSPoint)pointInWindow;
- (void)mouseEnterAtPoint:(NSPoint)pointInWindow;
- (void)mouseUpAtPoint:(NSPoint)pointInWindow;
- (void)mouseMoveToPoint:(NSPoint)pointInWindow withFlags:(NSEventModifierFlags)flags;
- (void)sendClicksAtPoint:(NSPoint)pointInWindow numberOfClicks:(NSUInteger)numberOfClicks;
- (NSWindow *)hostWindow;
- (void)typeCharacter:(char)character;
@end
#endif

#endif // WK_API_ENABLED

