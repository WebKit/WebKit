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
#import <WebCore/LayoutPoint.h>
#import <WebCore/LayoutUnit.h>
#import <WebCore/ResourceError.h>
#import <simd/simd.h>
#import <wtf/MainThread.h>
#import <wtf/MonotonicTime.h>

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
#endif

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVInlinePreview);

namespace WebKit {

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

WKModelView * ModelElementController::modelViewForModelIdentifier(ModelIdentifier modelIdentifier)
{
    if (!m_webPageProxy.preferences().modelElementEnabled())
        return nil;

    if (!is<RemoteLayerTreeDrawingAreaProxy>(m_webPageProxy.drawingArea()))
        return nil;

    auto* node = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_webPageProxy.drawingArea()).remoteLayerTreeHost().nodeForID(modelIdentifier.layerIdentifier);
    if (!node)
        return nil;

    return dynamic_objc_cast<WKModelView>(node->uiView());
}

ASVInlinePreview * ModelElementController::previewForModelIdentifier(ModelIdentifier modelIdentifier)
{
    return [modelViewForModelIdentifier(modelIdentifier) preview];
}

void ModelElementController::takeModelElementFullscreen(ModelIdentifier modelIdentifier)
{
    auto *presentingViewController = m_webPageProxy.uiClient().presentingViewController();
    if (!presentingViewController)
        return;

    auto modelView = modelViewForModelIdentifier(modelIdentifier);
    if (!modelView)
        return;

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
                [modelView.layer.superlayer.context addFence:fenceHandle];
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
                    [modelView.layer.superlayer.context addFence:dismissFenceHandle];
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

ASVInlinePreview * ModelElementController::previewForModelIdentifier(ModelIdentifier modelIdentifier)
{
    if (!m_webPageProxy.preferences().modelElementEnabled())
        return nullptr;

    return m_inlinePreviews.get(modelIdentifier.uuid).get();
}

void ModelElementController::modelElementDidCreatePreview(URL fileURL, String uuid, WebCore::FloatSize size, CompletionHandler<void(Expected<std::pair<String, uint32_t>, WebCore::ResourceError>)>&& completionHandler)
{
    if (!m_webPageProxy.preferences().modelElementEnabled()) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::errorDomainWebKitInternal, 0, { }, "Model element disabled"_s }));
        return;
    }

    auto nsUUID = adoptNS([[NSUUID alloc] initWithUUIDString:uuid]);
    auto preview = adoptNS([allocASVInlinePreviewInstance() initWithFrame:CGRectMake(0, 0, size.width(), size.height()) UUID:nsUUID.get()]);

    LOG(ModelElement, "Created remote preview with UUID %s.", uuid.utf8().data());

    auto iterator = m_inlinePreviews.find(uuid);
    if (iterator == m_inlinePreviews.end())
        m_inlinePreviews.set(uuid, preview);
    else
        iterator->value = preview;

    // FIXME: Why is this not just using normal URL -> NSURL conversion?
    auto url = adoptNS([[NSURL alloc] initFileURLWithPath:fileURL.fileSystemPath()]);

    RELEASE_ASSERT(isMainRunLoop());
    [preview setupRemoteConnectionWithCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, preview, uuid = WTFMove(uuid), url = WTFMove(url), completionHandler = WTFMove(completionHandler)] (NSError *contextError) mutable {
        if (contextError) {
            LOG(ModelElement, "Unable to create remote connection for uuid %s: %@.", uuid.utf8().data(), contextError.localizedDescription);

            callOnMainRunLoop([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler), error = WebCore::ResourceError { contextError }] () mutable {
                if (!weakThis)
                    return;

                completionHandler(makeUnexpected(error));
            });
            return;
        }

        LOG(ModelElement, "Established remote connection with UUID %s.", uuid.utf8().data());

        [preview preparePreviewOfFileAtURL:url.get() completionHandler:makeBlockPtr([weakThis = WTFMove(weakThis), preview, uuid = WTFMove(uuid), url = WTFMove(url), completionHandler = WTFMove(completionHandler)] (NSError *loadError) mutable {
            if (loadError) {
                LOG(ModelElement, "Unable to load file for uuid %s: %@.", uuid.utf8().data(), loadError.localizedDescription);

                callOnMainRunLoop([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler), error = WebCore::ResourceError { loadError }] () mutable {
                    if (!weakThis)
                        return;

                    completionHandler(makeUnexpected(error));
                });
                return;
            }

            LOG(ModelElement, "Loaded file with UUID %s.", uuid.utf8().data());

            auto contextId = [preview contextId];
            callOnMainRunLoop([weakThis = WTFMove(weakThis), uuid = WTFMove(uuid), completionHandler = WTFMove(completionHandler), contextId] () mutable {
                if (!weakThis)
                    return;

                completionHandler(std::make_pair(uuid, contextId));
            });
        }).get()];
    }).get()];
}

RetainPtr<ASVInlinePreview> ModelElementController::previewForUUID(const String& uuid)
{
    return m_inlinePreviews.get(uuid);
}

void ModelElementController::handleMouseDownForModelElement(const String& uuid, const WebCore::LayoutPoint& locationInPageCoordinates, MonotonicTime timestamp)
{
    if (auto preview = previewForUUID(uuid))
        [preview mouseDownAtLocation:CGPointMake(locationInPageCoordinates.x().toFloat(), locationInPageCoordinates.y().toFloat()) timestamp:timestamp.secondsSinceEpoch().value()];
}

void ModelElementController::handleMouseMoveForModelElement(const String& uuid, const WebCore::LayoutPoint& locationInPageCoordinates, MonotonicTime timestamp)
{
    if (auto preview = previewForUUID(uuid))
        [preview mouseDraggedAtLocation:CGPointMake(locationInPageCoordinates.x().toFloat(), locationInPageCoordinates.y().toFloat()) timestamp:timestamp.secondsSinceEpoch().value()];
}

void ModelElementController::handleMouseUpForModelElement(const String& uuid, const WebCore::LayoutPoint& locationInPageCoordinates, MonotonicTime timestamp)
{
    if (auto preview = previewForUUID(uuid))
        [preview mouseUpAtLocation:CGPointMake(locationInPageCoordinates.x().toFloat(), locationInPageCoordinates.y().toFloat()) timestamp:timestamp.secondsSinceEpoch().value()];
}

#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)

static bool previewHasCameraSupport(ASVInlinePreview *preview)
{
#if ENABLE(ARKIT_INLINE_PREVIEW_CAMERA_TRANSFORM)
    return [preview respondsToSelector:@selector(getCameraTransform:)];
#else
    return false;
#endif
}

void ModelElementController::getCameraForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<WebCore::HTMLModelElementCamera, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasCameraSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_CAMERA_TRANSFORM)
    [preview getCameraTransform:makeBlockPtr([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (simd_float3 cameraTransform, NSError *error) mutable {
        if (error) {
            callOnMainRunLoop([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)] () mutable {
                if (weakThis)
                    completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
            });
            return;
        }

        callOnMainRunLoop([cameraTransform, weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)] () mutable {
            if (weakThis)
                completionHandler(WebCore::HTMLModelElementCamera { cameraTransform.x, cameraTransform.y, cameraTransform.z });
        });
    }).get()];
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::setCameraForModelElement(ModelIdentifier modelIdentifier, WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasCameraSupport(preview)) {
        completionHandler(false);
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_CAMERA_TRANSFORM)
    [preview setCameraTransform:simd_make_float3(camera.pitch, camera.yaw, camera.scale)];
    completionHandler(true);
#else
    ASSERT_NOT_REACHED();
#endif
}

static bool previewHasAnimationSupport(ASVInlinePreview *preview)
{
#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    return [preview respondsToSelector:@selector(isPlaying)];
#else
    return false;
#endif
}

void ModelElementController::isPlayingAnimationForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    completionHandler([preview isPlaying]);
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::setAnimationIsPlayingForModelElement(ModelIdentifier modelIdentifier, bool isPlaying, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(false);
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    [preview setIsPlaying:isPlaying reply:makeBlockPtr([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (BOOL, NSError *error) mutable {
        callOnMainRunLoop([error, weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)] () mutable {
            if (weakThis)
                completionHandler(!error);
        });
    }).get()];
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::isLoopingAnimationForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    completionHandler([preview isLooping]);
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::setIsLoopingAnimationForModelElement(ModelIdentifier modelIdentifier, bool isLooping, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(false);
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    preview.isLooping = isLooping;
    completionHandler(true);
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::animationDurationForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    completionHandler(Seconds([preview duration]));
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::animationCurrentTimeForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    completionHandler(Seconds([preview currentTime]));
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::setAnimationCurrentTimeForModelElement(ModelIdentifier modelIdentifier, Seconds currentTime, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAnimationSupport(preview)) {
        completionHandler(false);
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_ANIMATIONS_CONTROL)
    preview.currentTime = currentTime.seconds();
    completionHandler(true);
#else
    ASSERT_NOT_REACHED();
#endif
}

static bool previewHasAudioSupport(ASVInlinePreview *preview)
{
#if ENABLE(ARKIT_INLINE_PREVIEW_AUDIO_CONTROL)
    return [preview respondsToSelector:@selector(hasAudio)];
#else
    return false;
#endif
}

void ModelElementController::hasAudioForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAudioSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_AUDIO_CONTROL)
    completionHandler([preview hasAudio]);
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::isMutedForModelElement(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAudioSupport(preview)) {
        completionHandler(makeUnexpected(WebCore::ResourceError { WebCore::ResourceError::Type::General }));
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_AUDIO_CONTROL)
    completionHandler([preview isMuted]);
#else
    ASSERT_NOT_REACHED();
#endif
}

void ModelElementController::setIsMutedForModelElement(ModelIdentifier modelIdentifier, bool isMuted, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* preview = previewForModelIdentifier(modelIdentifier);
    if (!previewHasAudioSupport(preview)) {
        completionHandler(false);
        return;
    }

#if ENABLE(ARKIT_INLINE_PREVIEW_AUDIO_CONTROL)
    preview.isMuted = isMuted;
    completionHandler(true);
#else
    ASSERT_NOT_REACHED();
#endif
}

#endif

}

#endif
