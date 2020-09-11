/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

@class _WKProcessPoolConfiguration;

#if PLATFORM(IOS_FAMILY)
@class _WKActivatedElementInfo;
@protocol UITextInputInternal;
@protocol UITextInputMultiDocument;
@protocol UITextInputPrivate;
@protocol UIWKInteractionViewProtocol;
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
@property (nonatomic, readonly) NSArray<NSString *> *tagsInBody;
- (void)loadTestPageNamed:(NSString *)pageName;
- (void)synchronouslyGoBack;
- (void)synchronouslyGoForward;
- (void)synchronouslyLoadHTMLString:(NSString *)html;
- (void)synchronouslyLoadHTMLString:(NSString *)html baseURL:(NSURL *)url;
- (void)synchronouslyLoadHTMLString:(NSString *)html preferences:(WKWebpagePreferences *)preferences;
- (void)synchronouslyLoadRequest:(NSURLRequest *)request;
- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName;
- (BOOL)_synchronouslyExecuteEditCommand:(NSString *)command argument:(NSString *)argument;
- (void)expectElementTagsInOrder:(NSArray<NSString *> *)tagNames;
- (void)expectElementCount:(NSInteger)count querySelector:(NSString *)querySelector;
- (void)expectElementTag:(NSString *)tagName toComeBefore:(NSString *)otherTagName;
- (NSString *)stringByEvaluatingJavaScript:(NSString *)script;
- (id)objectByEvaluatingJavaScriptWithUserGesture:(NSString *)script;
- (id)objectByEvaluatingJavaScript:(NSString *)script;
- (id)objectByCallingAsyncFunction:(NSString *)script withArguments:(NSDictionary *)arguments error:(NSError **)errorOut;
- (unsigned)waitUntilClientWidthIs:(unsigned)expectedClientWidth;
@end

@interface TestMessageHandler : NSObject <WKScriptMessageHandler>
- (void)addMessage:(NSString *)message withHandler:(dispatch_block_t)handler;
@end

@interface TestWKWebView : WKWebView
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration processPoolConfiguration:(_WKProcessPoolConfiguration *)processPoolConfiguration;
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration addToWindow:(BOOL)addToWindow;
- (void)synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:(NSString *)html;
- (void)clearMessageHandlers:(NSArray *)messageNames;
- (void)performAfterReceivingMessage:(NSString *)message action:(dispatch_block_t)action;
- (void)waitForMessage:(NSString *)message;

// This function waits until a DOM load event is fired.
// FIXME: Rename this function to better describe what "after loading" means.
- (void)performAfterLoading:(dispatch_block_t)actions;

- (void)waitForNextPresentationUpdate;
- (void)waitUntilActivityStateUpdateDone;
- (void)forceDarkMode;
- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName;
- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName;
- (void)collapseToStart;
- (void)collapseToEnd;
- (void)addToTestWindow;
- (BOOL)selectionRangeHasStartOffset:(int)start endOffset:(int)end;
- (void)clickOnElementID:(NSString *)elementID;
@end

#if PLATFORM(IOS_FAMILY)
@interface UIView (WKTestingUIViewUtilities)
- (UIView *)wkFirstSubviewWithClass:(Class)targetClass;
- (UIView *)wkFirstSubviewWithBoundsSize:(CGSize)size;
@end
#endif

#if PLATFORM(IOS_FAMILY)
@interface WKContentView : UIView
@end

@interface TestWKWebView (IOSOnly)
@property (nonatomic, readonly) UIView <UITextInputPrivate, UITextInputInternal, UITextInputMultiDocument, UIWKInteractionViewProtocol, UITextInputTokenizer> *textInputContentView;
@property (nonatomic, readonly) RetainPtr<NSArray> selectionRectsAfterPresentationUpdate;
@property (nonatomic, readonly) CGRect caretViewRectInContentCoordinates;
@property (nonatomic, readonly) NSArray<NSValue *> *selectionViewRectsInContentCoordinates;
- (_WKActivatedElementInfo *)activatedElementAtPosition:(CGPoint)position;
- (void)evaluateJavaScriptAndWaitForInputSessionToChange:(NSString *)script;
- (WKContentView *)wkContentView;
@end
#endif

#if PLATFORM(MAC)
@interface TestWKWebView (MacOnly)
// Simulates clicking with a pressure-sensitive device, if possible.
- (void)mouseDownAtPoint:(NSPoint)pointInWindow simulatePressure:(BOOL)simulatePressure;
- (void)mouseDownAtPoint:(NSPoint)pointInWindow simulatePressure:(BOOL)simulatePressure withFlags:(NSEventModifierFlags)flags eventType:(NSEventType)eventType;
- (void)mouseDragToPoint:(NSPoint)pointInWindow;
- (void)mouseEnterAtPoint:(NSPoint)pointInWindow;
- (void)mouseUpAtPoint:(NSPoint)pointInWindow;
- (void)mouseUpAtPoint:(NSPoint)pointInWindow withFlags:(NSEventModifierFlags)flags eventType:(NSEventType)eventType;
- (void)mouseMoveToPoint:(NSPoint)pointInWindow withFlags:(NSEventModifierFlags)flags;
- (void)sendClicksAtPoint:(NSPoint)pointInWindow numberOfClicks:(NSUInteger)numberOfClicks;
- (void)sendClickAtPoint:(NSPoint)pointInWindow;
- (NSWindow *)hostWindow;
- (void)typeCharacter:(char)character;
- (void)waitForPendingMouseEvents;
- (void)setEventTimestampOffset:(NSTimeInterval)offset;
@property (nonatomic, readonly) NSTimeInterval eventTimestamp;
@end
#endif

