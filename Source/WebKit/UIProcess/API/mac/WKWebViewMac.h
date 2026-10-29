/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "WKWebViewInternal.h"

#if PLATFORM(MAC)

#import "WebViewImpl.h"
#import <WebCore/ScrollTypes.h>
#import <WebKit/_WKOverlayScrollbarStyle.h>

_WKOverlayScrollbarStyle LEGACY_NODELETE toAPIScrollbarStyle(std::optional<WebCore::ScrollbarOverlayStyle>);
std::optional<WebCore::ScrollbarOverlayStyle> LEGACY_NODELETE toCoreScrollbarStyle(_WKOverlayScrollbarStyle);

@interface WKWebView (WKInternalMac)

- (NSTextInputContext *)_web_superInputContext;
- (void)_web_superQuickLookWithEvent:(NSEvent *)event;
- (void)_web_superRemoveTrackingRect:(NSTrackingRectTag)tag;
- (void)_web_superSwipeWithEvent:(NSEvent *)event;
- (void)_web_superMagnifyWithEvent:(NSEvent *)event;
- (void)_web_superSmartMagnifyWithEvent:(NSEvent *)event;
- (id)_web_superAccessibilityAttributeValue:(NSString *)attribute;
- (void)_web_superDoCommandBySelector:(SEL)selector;
- (BOOL)_web_superPerformKeyEquivalent:(NSEvent *)event;
- (void)_web_superKeyDown:(NSEvent *)event;
- (NSView *)_web_superHitTest:(NSPoint)point;

- (id)_web_immediateActionAnimationControllerForHitTestResultInternal:(API::HitTestResult*)hitTestResult withType:(uint32_t)type userData:(API::Object*)userData;
- (void)_web_prepareForImmediateActionAnimation;
- (void)_web_cancelImmediateActionAnimation;
- (void)_web_completeImmediateActionAnimation;

- (void)_web_dismissContentRelativeChildWindows;
- (void)_web_dismissContentRelativeChildWindowsWithAnimation:(BOOL)animate;
- (void)_web_editorStateDidChange;

- (void)_web_gestureEventWasNotHandledByWebCore:(NSEvent *)event;
- (void)_web_magnificationGestureEventWasNotHandledByWebCoreWithPhase:(NSEventPhase)phase magnification:(CGFloat)magnification locationInWindow:(NSPoint)locationInWindow;

- (void)_web_didChangeContentSize:(NSSize)newSize;

#if ENABLE(DRAG_SUPPORT)
- (WKDragDestinationAction)_web_dragDestinationActionForDraggingInfo:(id <NSDraggingInfo>)draggingInfo;
- (void)_web_didPerformDragOperation:(BOOL)handled;
#if ENABLE(DRAG_SOURCE_CUSTOMIZATION)
- (void)_web_draggingItemsForDraggingItem:(NSDraggingItem *)draggingItem atLocation:(NSPoint)viewLocation completionHandler:(void (^)(NSArray<NSDraggingItem *> *draggingItems))completionHandler;
- (NSDragOperation)_web_dragSourceOperationMaskForDraggingContext:(NSDraggingContext)context defaultMask:(NSDragOperation)defaultMask;
- (void)_web_draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)point;
- (void)_web_draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)point operation:(NSDragOperation)operation;
#endif
#endif

#if HAVE(TOUCH_BAR)
- (void)_web_didAddMediaControlsManager:(id)controlsManager;
- (void)_web_didRemoveMediaControlsManager;
#endif

- (BOOL)_web_hasActiveIntelligenceTextEffects;
- (void)_web_suppressContentRelativeChildViews;
- (void)_web_restoreContentRelativeChildViews;

- (void)_takeFindStringFromSelectionInternal:(id)sender;
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange;

- (void)_resetSecureInputState;
- (Vector<String>)_promisedFileMIMETypes:(id<NSDraggingInfo>)info;

@end

#endif // PLATFORM(MAC)
