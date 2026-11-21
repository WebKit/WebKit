/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WKAppKitGestureControllerCxxSupport.h"

#if HAVE(APPKIT_GESTURES_SUPPORT)

#import "AppKitGestureMomentumEventGenerator.h"
#import "DrawingAreaProxy.h"
#import "ImageAnalysisUtilities.h"
#import "InteractionInformationAtPosition.h"
#import "InteractionInformationRequest.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingTreeCocoa.h"
#import "TextRecognitionUpdateResult.h"
#import "WKWebView.h"
#import "WKWebViewInternal.h"
#import "WebEventFactory.h"
#import "WebPageProxy.h"
#import "WebViewImpl.h"
#import <WebCore/ElementContext.h>
#import <WebCore/EventRegion.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/IntPoint.h>
#import <WebCore/TextRecognitionResult.h>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedPtr.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

static bool isAnalyzableImageForLiveText(const WebKit::InteractionInformationAtPosition& info)
{
    return info.isImage && info.image && info.hostImageOrVideoElementContext && !info.isAnimatedImage && !info.isContentEditable;
}

@implementation WKAppKitGestureControllerCxxSupport {
    WeakObjCPtr<WKAppKitGestureController> _controller;
    BlockPtr<void(WebKit::InteractionInformationAtPosition)> _positionInformationDidChangeHandler;
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    std::unique_ptr<WebKit::AppKitGestureMomentumEventGenerator> _momentumEventGenerator;
#endif
}

- (WKAppKitGestureController *)controller
{
    return _controller.getAutoreleased();
}

- (void)setController:(WKAppKitGestureController *)controller
{
    _controller = controller;
}

- (void)setPositionInformationDidChangeHandler:(void (^)(WebKit::InteractionInformationAtPosition))handler
{
    _positionInformationDidChangeHandler = makeBlockPtr(handler);
}

- (void)positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info
{
    // Resolving queued callbacks can re-enter into client code (e.g. UI delegate callbacks), which
    // may release the WKWebView and tear the controller down before the handler returns.
    RetainPtr protectedController = _controller.get();
    // The handler stays alive until it returns, even if it is replaced while it runs.
    auto handler = _positionInformationDidChangeHandler;
    RELEASE_ASSERT(handler);
    handler(info);
}

- (void)preflightImageAnalysis:(WebKit::InteractionInformationAtPosition)info completion:(void (^)(BOOL foundText, WebCore::ElementContext element))completion
{
    RetainPtr webView = [_controller.get() webView];
    if (!webView)
        return;

    if (!isAnalyzableImageForLiveText(info)) {
        // Not an analyzable image: leave both image-analysis deferrals with the default
        // NotApplicable outcome, so text selection and the drag / context-menu fallback each
        // resolve on their own (non-image) merits.
        return;
    }

    // This dereference is guaranteed to be non-nil due to the `isAnalyzableImageForLiveText` check above.
    const auto elementContext = *info.hostImageOrVideoElementContext;

    // This dereference is guaranteed to be non-nil due to the `isAnalyzableImageForLiveText` check above.
    RetainPtr cgImage = info.image->createPlatformImage();
    if (!cgImage) {
        // An image we can't rasterize: treat it as an image without text so the press falls
        // through to drag / context menu.
        completion(NO, elementContext);
        return;
    }

    RELEASE_LOG(ImageAnalysis, "Image analysis preflight gesture initiated.");

    const auto requestLocation = info.request.point;

    RetainPtr analyzerRequest = WebKit::createImageAnalyzerRequest(cgImage.get(), VKAnalysisTypeText);
    const auto startTime = MonotonicTime::now();
    protect([webView _impl])->processImageAnalyzerRequest(analyzerRequest.get(), [weakSelf = WeakObjCPtr<WKAppKitGestureControllerCxxSupport>(self), elementContext, requestLocation, completion = makeBlockPtr(completion), startTime](RetainPtr<VKCImageAnalysis>&& result, NSError *) mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        RetainPtr webView = [strongSelf->_controller.get() webView];
        if (!webView)
            return;

        auto hasTextResults = [result hasResultsForAnalysisTypes:VKAnalysisTypeText];
        RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (found text? %d)", (MonotonicTime::now() - startTime).milliseconds(), hasTextResults);

        [webView _protectedPage]->updateWithTextRecognitionResult(WebKit::makeTextRecognitionResult(result.get()), elementContext, requestLocation, [elementContext, completion = WTF::move(completion)](const auto& updateResult) mutable {
            // Text found and injected as an image overlay -> allow the deferred text selection and
            // prevent the drag / context-menu fallback (Live Text wins). Otherwise -> the reverse.
            completion(updateResult == WebKit::TextRecognitionUpdateResult::Text, elementContext);
        });
    });
}

- (uint8_t)touchActionsAtPoint:(NSPoint)point
{
    OptionSet<WebCore::TouchAction> defaultTouchActions { WebCore::TouchAction::Auto };

    RetainPtr webView = [_controller.get() webView];
    if (!webView)
        return defaultTouchActions.toRaw();

    RefPtr remoteDrawingArea = dynamicDowncast<WebKit::RemoteLayerTreeDrawingAreaProxy>([webView _protectedPage]->drawingArea());
    if (!remoteDrawingArea)
        return defaultTouchActions.toRaw();

    RetainPtr rootLayer = remoteDrawingArea->remoteLayerTreeHost().rootLayer();
    if (!rootLayer)
        return defaultTouchActions.toRaw();

    WebCore::FloatPoint location { point };
    return WebKit::eventRegionForPoint(rootLayer.get(), location).transform([&location](const WebCore::EventRegion& eventRegion) {
        return eventRegion.touchActionsForPoint(WebCore::roundedIntPoint(location));
    }).value_or(defaultTouchActions).toRaw();
}

- (WebKit::OptionalTransactionID)lastCommittedMainFrameLayerTreeTransactionID
{
    RetainPtr webView = [_controller.get() webView];
    if (!webView)
        return { };

    RefPtr drawingArea = [webView _protectedPage]->drawingArea();
    if (!drawingArea)
        return { };

    RefPtr remoteDrawingArea = dynamicDowncast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*drawingArea);
    if (!remoteDrawingArea)
        return { };

    return remoteDrawingArea->lastCommittedMainFrameLayerTreeTransactionID();
}

- (void)doAfterProcessingAllPendingMouseEvents:(void (^)(void))completion
{
    RetainPtr webView = [_controller.get() webView];
    if (!webView)
        return;

    [webView _protectedPage]->doAfterProcessingAllPendingMouseEvents([completion = makeBlockPtr(completion)] {
        completion();
    });
}

- (void)resetMomentum
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    _momentumEventGenerator = nullptr;
#endif
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
namespace WebKit::CxxInteropSupport {

AppKitGestureMomentumEventGenerator* momentumEventGenerator(WKAppKitGestureControllerCxxSupport *support)
{
    return support->_momentumEventGenerator.get();
}

AppKitGestureMomentumEventGenerator& ensureMomentumEventGenerator(WKAppKitGestureControllerCxxSupport *support)
{
    if (!support->_momentumEventGenerator)
        support->_momentumEventGenerator = makeUnique<AppKitGestureMomentumEventGenerator>(support->_controller.get());
    return *support->_momentumEventGenerator;
}

} // namespace WebKit::CxxInteropSupport
#endif

@end

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
