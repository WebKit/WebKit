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

#ifdef __cplusplus
#import <wtf/Forward.h>
#import <wtf/IterationStatus.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>
#endif

@class _WKContextMenuElementInfo;
@class _WKFrameTreeNode;
@class _WKJSHandle;
@class _WKProcessPoolConfiguration;

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#import "WKBrowserEngineDefinitions.h"
@class _WKActivatedElementInfo;
@class _WKTextInputContext;
@class UITextSuggestion;
@class UIWKDocumentContext;
@class UIWKDocumentRequest;
@protocol UITextInputInternal;
@protocol UITextInputMultiDocument;
@protocol UITextInputPrivate;
@protocol UITextInputTraits_Private;
@protocol UIWKInteractionViewProtocol_Staging_95652872;
@protocol BETextInput;
@protocol BEExtendedTextInputTraits;
#endif

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface WKWebView (AdditionalDeclarations)
#if PLATFORM(MAC)
- (void)copy:(nullable id)sender;
- (void)paste:(nullable id)sender;
- (void)changeAttributes:(nullable id)sender;
- (void)changeColor:(nullable id)sender;
- (void)superscript:(nullable id)sender;
- (void)subscript:(nullable id)sender;
- (void)unscript:(nullable id)sender;
#endif
@end

#ifdef __cplusplus

namespace TestWebKitAPI {

struct AutocorrectionContext {
    String contextBeforeSelection;
    String selectedText;
    String contextAfterSelection;
    String markedText;
    NSRange selectedRangeInMarkedText;
};

} // namespace TestWebKitAPI

namespace WebCore {
class Color;
}

#endif

@interface WKWebView (TestWebKitAPI)
#if PLATFORM(IOS_FAMILY)
@property (nonatomic, readonly) CGRect selectionClipRect;
@property (nonatomic, readonly) BOOL hasAsyncTextInput;
#if USE(BROWSERENGINEKIT)
@property (nonatomic, readonly, nullable) id<BETextInput> asyncTextInput;
@property (nonatomic, readonly, nullable) id<BEExtendedTextInputTraits> extendedTextInputTraits;
#endif
#if HAVE(UI_WK_DOCUMENT_CONTEXT)
- (void)synchronouslyAdjustSelectionWithDelta:(NSRange)range NS_SWIFT_UNAVAILABLE("Spins the run loop");
#endif
@property (nonatomic, readonly, nullable) id<UITextInputTraits_Private> effectiveTextInputTraits;
#ifdef __cplusplus
@property (nonatomic, readonly) TestWebKitAPI::AutocorrectionContext autocorrectionContext;
- (std::pair<CGRect, CGRect>)autocorrectionRectsForString:(NSString *)string;
#endif
- (nullable NSArray<_WKTextInputContext *> *)synchronouslyRequestTextInputContextsInRect:(CGRect)rect NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)replaceText:(NSString *)input withText:(NSString *)correction shouldUnderline:(BOOL)shouldUnderline completion:(void(^)())completion;
- (void)insertText:(NSString *)primaryString alternatives:(NSArray<NSString *> *)alternatives;
- (void)handleKeyEvent:(WebEvent *)event completion:(void (^)(WebEvent *theEvent, BOOL handled))completion;
- (void)selectTextForContextMenuWithLocationInView:(CGPoint)locationInView completion:(void(^)(BOOL shouldPresent))completion;
- (void)selectTextInGranularity:(UITextGranularity)granularity atPoint:(CGPoint)locationInView NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)defineSelection;
- (void)shareSelection;
- (void)moveSelectionToStartOfParagraph;
- (void)extendSelectionToStartOfParagraph;
- (void)moveSelectionToEndOfParagraph;
- (void)extendSelectionToEndOfParagraph;
- (void)insertTextSuggestion:(UITextSuggestion *)textSuggestion;
- (void)focusInWindow;
#if HAVE(UI_WK_DOCUMENT_CONTEXT)
- (nullable UIWKDocumentContext *)synchronouslyRequestDocumentContext:(UIWKDocumentRequest *)request NS_SWIFT_UNAVAILABLE("Spins the run loop");
#endif
#endif // PLATFORM(IOS_FAMILY)

- (nullable CALayer *)firstLayerWithName:(NSString *)layerName;
- (nullable CALayer *)firstLayerWithNameContaining:(NSString *)layerName;
#ifdef __cplusplus
- (void)forEachCALayer:(IterationStatus(^)(CALayer *))visitor;
#endif

@property (nonatomic, readonly, nullable) CGImageRef snapshotAfterScreenUpdates NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly) NSUInteger gpuToWebProcessConnectionCount NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly) NSUInteger modelProcessModelPlayerCount NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly, nullable) NSString *contentsAsString NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly, nullable) NSData *contentsAsWebArchive NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly, nullable) NSArray<NSString *> *tagsInBody NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly) NSString *selectedText NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)loadTestPageNamed:(NSString *)pageName;
- (void)synchronouslyGoBack NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)synchronouslyGoForward NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)synchronouslyLoadHTMLString:(NSString *)html NS_SWIFT_UNAVAILABLE("Use async load(html:baseURL:) instead");
- (void)synchronouslyLoadHTMLString:(NSString *)html baseURL:(nullable NSURL *)url NS_SWIFT_UNAVAILABLE("Use async load(html:baseURL:) instead");
- (void)synchronouslyLoadHTMLString:(NSString *)html preferences:(WKWebpagePreferences *)preferences NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)synchronouslyLoadRequest:(NSURLRequest *)request NS_SWIFT_UNAVAILABLE("Use async loadAndWait(_:) instead");
- (void)synchronouslyLoadSimulatedRequest:(NSURLRequest *)request responseHTMLString:(NSString *)htmlString NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)synchronouslyLoadRequest:(NSURLRequest *)request preferences:(WKWebpagePreferences *)preferences NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)synchronouslyLoadRequestIgnoringSSLErrors:(NSURLRequest *)request NS_SWIFT_UNAVAILABLE("Use async loadAndWaitIgnoringSSLErrors(_:) instead");
- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName NS_SWIFT_UNAVAILABLE("Use async load(testPageNamed:) instead");
- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName asStringWithBaseURL:(nullable NSURL *)url NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)synchronouslyLoadTestPageNamed:(NSString *)pageName preferences:(WKWebpagePreferences *)preferences NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (BOOL)_synchronouslyExecuteEditCommand:(NSString *)command argument:(nullable NSString *)argument NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)expectElementTagsInOrder:(NSArray<NSString *> *)tagNames NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)expectElementCount:(NSInteger)count querySelector:(NSString *)querySelector NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)expectElementTag:(NSString *)tagName toComeBefore:(NSString *)otherTagName NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (BOOL)evaluateMediaQuery:(NSString *)query NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (NSString *)stringByEvaluatingJavaScript:(NSString *)script NS_SWIFT_UNAVAILABLE("Use async callJavaScript(returning:_:) instead");
- (NSString *)stringByEvaluatingJavaScript:(NSString *)script inFrame:(nullable WKFrameInfo *)frame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable id)objectByEvaluatingJavaScriptWithUserGesture:(NSString *)script NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable id)objectByEvaluatingJavaScript:(NSString *)script NS_SWIFT_UNAVAILABLE("Use async callJavaScript(returning:_:) instead");
- (nullable id)objectByEvaluatingJavaScript:(NSString *)script inFrame:(nullable WKFrameInfo *)frame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable id)objectByEvaluatingJavaScript:(NSString *)script inFrame:(nullable WKFrameInfo *)frame inContentWorld:(WKContentWorld *)world NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable id)objectByEvaluatingJavaScriptWithUserGesture:(NSString *)script inFrame:(nullable WKFrameInfo *)frame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable id)objectByCallingAsyncFunction:(NSString *)script withArguments:(nullable NSDictionary *)arguments NS_SWIFT_UNAVAILABLE("Use async callJavaScript(returning:_:) instead");
- (nullable id)objectByCallingAsyncFunction:(NSString *)script withArguments:(nullable NSDictionary *)arguments error:(NSError **)errorOut NS_SWIFT_UNAVAILABLE("Use async callJavaScript(returning:_:) instead");
- (nullable id)objectByCallingAsyncFunction:(NSString *)script withArguments:(nullable NSDictionary *)arguments inFrame:(nullable WKFrameInfo *)frame inContentWorld:(WKContentWorld *)world NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (unsigned)waitUntilClientWidthIs:(unsigned)expectedClientWidth NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (CGRect)elementRectFromSelector:(NSString *)selector NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (CGPoint)elementMidpointFromSelector:(NSString *)selector NS_SWIFT_UNAVAILABLE("Use async elementMidpoint(selector:) instead");
- (nullable _WKJSHandle *)querySelector:(NSString *)selector frame:(nullable WKFrameInfo *)frame world:(WKContentWorld *)world NS_SWIFT_UNAVAILABLE("Use async querySelector(_:in:frame:) instead");
- (void)visitUnsafeSite;
@end

@interface WKWebView (TestWebKitAPI_NonCpp)

#if PLATFORM(IOS_FAMILY)
@property (nonatomic, readonly, nullable) UIView <UITextInputPrivate, UITextInputInternal, UITextInputMultiDocument, UIWKInteractionViewProtocol_Staging_95652872, UITextInputTokenizer> *textInputContentView;
#endif

@end

NS_SWIFT_UI_ACTOR
@interface TestMessageHandler : NSObject <WKScriptMessageHandler>
- (void)addMessage:(NSString *)message withHandler:(dispatch_block_t)handler;
@property (nonatomic, copy, nullable) void (^didReceiveScriptMessage)(NSString *);
@property (nonatomic, readonly, nullable) NSArray<NSString *> *receivedMessages;
@end

@interface TestWKWebView : WKWebView
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration processPoolConfiguration:(_WKProcessPoolConfiguration *)processPoolConfiguration;
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration addToWindow:(BOOL)addToWindow;
- (void)synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:(NSString *)html NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)clearMessageHandlers:(NSArray *)messageNames;
- (void)performAfterReceivingMessage:(NSString *)message action:(dispatch_block_t)action;
- (void)performAfterReceivingAnyMessage:(void (^)(NSString *))action;
- (void)waitForMessage:(NSString *)message NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)waitForMessages:(NSArray<NSString *> *)messages NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)waitForMessagesUnordered:(NSArray<NSString *> *)messages NS_SWIFT_UNAVAILABLE("Spins the run loop");

// This function waits until a DOM load event is fired.
// FIXME: Rename this function to better describe what "after loading" means.
- (void)performAfterLoading:(dispatch_block_t)actions;

- (void)waitForNextPresentationUpdate NS_SWIFT_UNAVAILABLE("Use async nextPresentationUpdate() instead");
- (void)waitForNextVisibleContentRectUpdate NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)waitUntilActivityStateUpdateDone NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)forceLightMode;
- (void)forceDarkMode;
- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)collapseToStart;
- (void)collapseToEnd;
- (void)addToTestWindow;
- (void)removeFromTestWindow;
- (BOOL)selectionRangeHasStartOffset:(int)start endOffset:(int)end NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (BOOL)selectionRangeHasStartOffset:(int)start endOffset:(int)end inFrame:(nullable WKFrameInfo *)frameInfo NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)clickOnElementID:(NSString *)elementID;
- (void)waitForPendingMouseEvents NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)focus;
#ifdef __cplusplus
- (std::optional<CGPoint>)getElementMidpoint:(NSString *)selector;
- (Vector<WebCore::Color>)sampleColors;
- (Vector<WebCore::Color>)sampleColorsInRect:(CGRect)rect;
- (Vector<WebCore::Color>)sampleColorsWithInterval:(unsigned)interval;
- (RetainPtr<_WKFrameTreeNode>)frameTree;
#endif
- (void)typeCharacter:(char)character;
- (void)setVisibility:(BOOL)isVisible NS_SWIFT_UNAVAILABLE("Spins the run loop");
@end

#if PLATFORM(IOS_FAMILY)
@interface UIView (WKTestingUIViewUtilities)
- (nullable UIView *)wkFirstSubviewWithClass:(Class)targetClass;
- (nullable UIView *)wkFirstSubviewWithBoundsSize:(CGSize)size;
@end
#endif

#if PLATFORM(IOS_FAMILY)
@interface WKContentView : UIView
@end

@interface TestWKWebView (IOSOnly)
@property (nonatomic) UIEdgeInsets overrideSafeAreaInset;
@property (nonatomic, readonly) CGRect caretViewRectInContentCoordinates;
@property (nonatomic, readonly) NSArray<NSValue *> *selectionViewRectsInContentCoordinates;
@property (nonatomic, readonly, nullable) NSString *textForSpeakSelection NS_SWIFT_UNAVAILABLE("Spins the run loop");
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
@property (nonatomic, readonly, nullable) UIView *selectionHighlightView;
#endif
- (nullable _WKActivatedElementInfo *)activatedElementAtPosition:(CGPoint)position NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)evaluateJavaScriptAndWaitForInputSessionToChange:(NSString *)script NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)evaluateJavaScriptAndWaitForInputSessionToChange:(NSString *)script inFrame:(nullable WKFrameInfo *)frame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable WKContentView *)wkContentView;
- (void)setZoomScaleSimulatingUserTriggeredZoom:(CGFloat)zoomScale;
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
- (void)rightClickAtPoint:(NSPoint)pointInWindow;
- (void)wheelEventAtPoint:(CGPoint)pointInWindow wheelDelta:(CGSize)delta;
- (void)wheelEventAtPoint:(CGPoint)pointInWindow wheelDelta:(CGSize)delta phase:(CGScrollPhase)phase momentumPhase:(CGMomentumScrollPhase)momentumPhase;
- (BOOL)acceptsFirstMouseAtPoint:(NSPoint)pointInWindow;
- (nullable NSWindow *)hostWindow;
- (nullable NSEvent *)_mouseEventWithType:(NSEventType)type atLocation:(NSPoint)pointInWindow;
- (void)typeCharacter:(char)character modifiers:(NSEventModifierFlags)modifiers;
- (void)sendKey:(NSString *)characters code:(unsigned short)keyCode isDown:(BOOL)isDown modifiers:(NSEventModifierFlags)modifiers;
- (void)setEventTimestampOffset:(NSTimeInterval)offset;
@property (nonatomic, readonly) NSArray<NSString *> *collectLogsForNewConnections NS_SWIFT_UNAVAILABLE("Spins the run loop");
@property (nonatomic, readonly) NSTimeInterval eventTimestamp;
@property (nonatomic) BOOL forceWindowToBecomeKey;
@end
#endif

@interface TestWKWebView (SiteIsolation)
- (_WKFrameTreeNode *)mainFrame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable WKFrameInfo *)firstChildFrame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (WKFrameInfo *)secondChildFrame NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (void)evaluateJavaScript:(NSString *)string inFrame:(nullable WKFrameInfo *)frame completionHandler:(nullable void(^)(id _Nullable, NSError * _Nullable))completionHandler;
- (WKFindResult *)findStringAndWait:(NSString *)string withConfiguration:(WKFindConfiguration *)configuration NS_SWIFT_UNAVAILABLE("Spins the run loop");
@end

#if PLATFORM(MAC)
typedef BOOL (^MenuItemFilter)(NSMenuItem *);
#endif

@interface TestWKWebView (ContextMenu)
#if PLATFORM(MAC)
- (void)rightClick:(NSPoint)clickLocation andSelectItemMatching:(MenuItemFilter)filter NS_SWIFT_UNAVAILABLE("Spins the run loop");
- (nullable _WKContextMenuElementInfo *)rightClickAtPointAndWaitForContextMenu:(NSPoint)clickLocation NS_SWIFT_UNAVAILABLE("Spins the run loop");
#endif
@end

NS_HEADER_AUDIT_END(nullability, sendability)
