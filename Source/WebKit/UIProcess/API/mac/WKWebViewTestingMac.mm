/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebViewPrivateForTestingMac.h"

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "AudioSessionRoutingArbitratorProxy.h"
#import "WKNSData.h"
#import "WKWebViewMac.h"
#import "WebColorPicker.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import "_WKFrameHandleInternal.h"
#import <WebCore/ColorCocoa.h>

@implementation WKWebView (WKTestingMac)

- (void)_requestControlledElementID
{
    if (_page)
        _page->requestControlledElementID();
}

- (void)_handleControlledElementIDResponse:(NSString *)identifier
{
    // Overridden by subclasses.
}

- (void)_handleAcceptedCandidate:(NSTextCheckingResult *)candidate
{
    _impl->handleAcceptedCandidate(candidate);
}

- (void)_didHandleAcceptedCandidate
{
    // Overridden by subclasses.
}

- (void)_didUpdateCandidateListVisibility:(BOOL)visible
{
    // Overridden by subclasses.
}

- (void)_forceRequestCandidates
{
    _impl->forceRequestCandidatesForTesting();
}

- (BOOL)_shouldRequestCandidates
{
    return _impl->shouldRequestCandidates();
}

- (BOOL)_allowsInlinePredictions
{
#if HAVE(INLINE_PREDICTIONS)
    return _impl->allowsInlinePredictions();
#else
    return NO;
#endif
}

- (void)_insertText:(id)string replacementRange:(NSRange)replacementRange
{
    [self insertText:string replacementRange:replacementRange];
}

- (NSRect)_candidateRect
{
    if (!_page->editorState().postLayoutData)
        return NSZeroRect;
    return _page->editorState().postLayoutData->selectionBoundingRect;
}

- (NSSet<NSView *> *)_pdfHUDs
{
    return _impl->pdfHUDs().autorelease();
}

- (NSMenu *)_activeMenu
{
    if (RetainPtr contextMenu = _page->activeContextMenu())
        return contextMenu.autorelease();
    if (RetainPtr domPasteMenu = _impl->domPasteMenu())
        return domPasteMenu.autorelease();
    return nil;
}

- (void)_retrieveAccessibilityTreeData:(void (^)(NSData *, NSError *))completionHandler
{
    _page->getAccessibilityTreeData([completionHandler = makeBlockPtr(completionHandler)] (API::Data* data) {
        completionHandler(protectedWrapper(data).get(), nil);
    });
}

- (BOOL)_secureEventInputEnabledForTesting
{
    return _impl->inSecureInputState();
}

- (NSRect)_windowRelativeBoundsForCustomSwipeViewsForTesting
{
    return _impl->windowRelativeBoundsForCustomSwipeViews();
}

- (void)_setSelectedColorForColorPicker:(NSColor *)color
{
    _page->checkedColorPickerClient()->didChooseColor(WebCore::colorFromCocoaColor(color));
}

- (void)_createFlagsChangedEventMonitorForTesting
{
    _impl->createFlagsChangedEventMonitor();
}

- (BOOL)_hasFlagsChangedEventMonitorForTesting
{
    return _impl->hasFlagsChangedEventMonitor();
}

@end

#endif // PLATFORM(MAC)
