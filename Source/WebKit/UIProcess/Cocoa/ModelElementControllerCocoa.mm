/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "ModelElementController.h"

#if ENABLE(ARKIT_INLINE_PREVIEW)

#import "Logging.h"
#import "WebPageProxy.h"

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#import "APIUIClient.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeViews.h"
#import "WKModelView.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/SystemPreviewSPI.h>
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#import <pal/spi/mac/SystemPreviewSPI.h>
#import <wtf/MainThread.h>
#endif

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVInlinePreview);

namespace WebKit {

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

void ModelElementController::takeModelElementFullscreen(WebCore::GraphicsLayer::PlatformLayerID contentLayerId)
{
    if (!m_webPageProxy.preferences().modelElementEnabled())
        return;

    if (!is<RemoteLayerTreeDrawingAreaProxy>(m_webPageProxy.drawingArea()))
        return;

    auto* node = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_webPageProxy.drawingArea()).remoteLayerTreeHost().nodeForID(contentLayerId);
    if (!node)
        return;

    auto *view = node->uiView();
    if (!view)
        return;

    if (![view isKindOfClass:[WKModelView class]])
        return;

    auto *presentingViewController = m_webPageProxy.uiClient().presentingViewController();
    if (!presentingViewController)
        return;

    WKModelView *modelView = (WKModelView *)view;
    CGRect initialFrame = [modelView convertRect:modelView.frame toView:nil];

    ASVInlinePreview *preview = [modelView preview];
    NSDictionary *previewOptions = @{@"WebKit": @"Model element fullscreen"};
    [preview createFullscreenInstanceWithInitialFrame:initialFrame previewOptions:previewOptions completionHandler:^(UIViewController *remoteViewController, CAFenceHandle *fenceHandle, NSError *creationError) {
        if (creationError) {
            LOG(ModelElement, "Unable to create fullscreen instance: %@", [creationError localizedDescription]);
            [fenceHandle invalidate];
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            remoteViewController.modalPresentationStyle = UIModalPresentationOverFullScreen;
            remoteViewController.view.backgroundColor = UIColor.clearColor;

            [presentingViewController presentViewController:remoteViewController animated:NO completion:^(void) {
                [CATransaction begin];
                [view.layer.superlayer.context addFence:fenceHandle];
                [CATransaction commit];
                [fenceHandle invalidate];
            }];

            [preview observeDismissFullscreenWithCompletionHandler:^(CAFenceHandle *dismissFenceHandle, NSDictionary *payload, NSError *dismissError) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (dismissError || !dismissFenceHandle) {
                        LOG(ModelElement, "Unable to get fence handle when dismissing fullscreen instance: %@", [dismissError localizedDescription]);
                        [dismissFenceHandle invalidate];
                        return;
                    }

                    [CATransaction begin];
                    [view.layer.superlayer.context addFence:dismissFenceHandle];
                    [CATransaction setCompletionBlock:^{
                        [remoteViewController dismissViewControllerAnimated:NO completion:nil];
                    }];
                    [CATransaction commit];
                    [dismissFenceHandle invalidate];
                });
            }];
        });
    }];
}

#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)

void ModelElementController::modelElementDidCreatePreview(const WebCore::ElementContext& context, const URL& fileURL, const String& uuid, const WebCore::FloatSize& size)
{
    if (!m_webPageProxy.preferences().modelElementEnabled())
        return;

    auto preview = adoptNS([allocASVInlinePreviewInstance() initWithFrame:CGRectMake(0, 0, size.width(), size.height()) UUID:[[NSUUID alloc] initWithUUIDString:uuid]]);

    LOG(ModelElement, "Created remote preview with UUID %s.", uuid.utf8().data());

    auto iterator = m_inlinePreviews.find(uuid);
    if (iterator == m_inlinePreviews.end())
        m_inlinePreviews.set(uuid, preview);
    else
        iterator->value = preview;

    RELEASE_ASSERT(isMainRunLoop());
    WeakPtr weakThis { *this };
    auto elementContextCopy = context;
    auto uuidCopy = uuid;
    NSURL *url = [NSURL fileURLWithPath:fileURL.fileSystemPath()];
    [preview setupRemoteConnectionWithCompletionHandler:^(NSError * _Nullable contextError) {
        if (contextError) {
            LOG(ModelElement, "Unable to create remote connection for uuid %s: %@.", uuidCopy.utf8().data(), [contextError localizedDescription]);
            return;
        }

        LOG(ModelElement, "Established remote connection with UUID %s.", uuidCopy.utf8().data());

        [preview preparePreviewOfFileAtURL:url completionHandler:^(NSError * _Nullable loadError) {
            if (loadError) {
                LOG(ModelElement, "Unable to load file for uuid %s: %@.", uuidCopy.utf8().data(), [loadError localizedDescription]);
                return;
            }

            LOG(ModelElement, "Loaded file with UUID %s.", uuidCopy.utf8().data());

            callOnMainRunLoop([weakThis, elementContextCopy, uuidCopy, contextId = [preview contextId]]() mutable {
                weakThis->m_webPageProxy.modelElementPreviewDidObtainContextId(elementContextCopy, uuidCopy, contextId);
            });
        }];
    }];
}

#endif

}

#endif
