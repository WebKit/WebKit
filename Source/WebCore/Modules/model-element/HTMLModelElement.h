/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Platform.h>

#if ENABLE(MODEL_ELEMENT)

#include <WebCore/ActiveDOMObject.h>
#include <WebCore/CachedRawResource.h>
#include <WebCore/CachedRawResourceClient.h>
#include <WebCore/CachedResourceHandle.h>
#include <WebCore/EventLoop.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLModelElementCamera.h>
#include <WebCore/IDLTypes.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerClient.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/VisibilityChangeClient.h>
#include <wtf/UniqueRef.h>

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
#include <WebCore/StageModeOperations.h>
#endif

namespace WebCore {

class CachedResourceRequest;
class DOMMatrixReadOnly;
class DOMPointReadOnly;
class Event;
class Exception;
class GraphicsLayer;
class LayoutPoint;
class LayoutSize;
class Model;
class ModelPlayerProvider;
class MouseEvent;

template<typename IDLType> class DOMPromiseDeferred;
template<typename IDLType> class DOMPromiseProxy;
template<typename IDLType> class DOMPromiseProxyWithResolveCallback;
template<typename> class ExceptionOr;

class HTMLModelElement final : public HTMLElement, private CachedRawResourceClient, public ModelPlayerClient, public ActiveDOMObject, public VisibilityChangeClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(HTMLModelElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLModelElement);
public:
    USING_CAN_MAKE_WEAKPTR(HTMLElement);

    static Ref<HTMLModelElement> create(const QualifiedName&, Document&);
    virtual ~HTMLModelElement();

    // ActiveDOMObject.
    void ref() const final { HTMLElement::ref(); }
    void deref() const final { HTMLElement::deref(); }

    // VisibilityChangeClient.
    void visibilityStateChanged() final;

    void sourcesChanged();
    const URL& currentSrc() const { return m_sourceURL; }
    bool complete() const { return m_dataComplete; }

    void configureGraphicsLayer(GraphicsLayer&, Color backgroundColor);

    std::optional<PlatformLayerIdentifier> layerID() const;

    // MARK: DOM Functions and Attributes

    using ReadyPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<HTMLModelElement>>;
    ReadyPromise& ready() { return m_readyPromise.get(); }

    WEBCORE_EXPORT RefPtr<Model> model() const;

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    const DOMMatrixReadOnly& entityTransform() const;
    ExceptionOr<void> setEntityTransform(const DOMMatrixReadOnly&);
#endif

#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    const DOMPointReadOnly& boundingBoxCenter() const;
    const DOMPointReadOnly& boundingBoxExtents() const;
#endif

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    using EnvironmentMapPromise = DOMPromiseProxy<IDLUndefined>;
    EnvironmentMapPromise& environmentMapReady() { return m_environmentMapReadyPromise.get(); }

    const URL& environmentMap() const;
    void setEnvironmentMap(const URL&);
#endif

    void enterFullscreen();

    using CameraPromise = DOMPromiseDeferred<IDLDictionary<HTMLModelElementCamera>>;
    void getCamera(CameraPromise&&);
    void setCamera(HTMLModelElementCamera, DOMPromiseDeferred<void>&&);

    using IsPlayingAnimationPromise = DOMPromiseDeferred<IDLBoolean>;
    void isPlayingAnimation(IsPlayingAnimationPromise&&);
    void playAnimation(DOMPromiseDeferred<void>&&);
    void pauseAnimation(DOMPromiseDeferred<void>&&);

    using IsLoopingAnimationPromise = DOMPromiseDeferred<IDLBoolean>;
    void isLoopingAnimation(IsLoopingAnimationPromise&&);
    void setIsLoopingAnimation(bool, DOMPromiseDeferred<void>&&);

    using DurationPromise = DOMPromiseDeferred<IDLDouble>;
    void animationDuration(DurationPromise&&);
    using CurrentTimePromise = DOMPromiseDeferred<IDLDouble>;
    void animationCurrentTime(CurrentTimePromise&&);
    void setAnimationCurrentTime(double, DOMPromiseDeferred<void>&&);

    using HasAudioPromise = DOMPromiseDeferred<IDLBoolean>;
    void hasAudio(HasAudioPromise&&);
    using IsMutedPromise = DOMPromiseDeferred<IDLBoolean>;
    void isMuted(IsMutedPromise&&);
    void setIsMuted(bool, DOMPromiseDeferred<void>&&);

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    bool immersive() const;
    void requestImmersive(DOMPromiseDeferred<void>&&);
    void ensureImmersivePresentation(CompletionHandler<void(ExceptionOr<const LayerHostingContextIdentifier>)>&&);
    void exitImmersivePresentation(CompletionHandler<void()>&&);
#endif

    bool supportsDragging() const;
    bool isDraggableIgnoringAttributes() const final;

    bool isInteractive() const;

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)
    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double);
    double duration() const;
    bool paused() const;
    void play(DOMPromiseDeferred<void>&&);
    void pause(DOMPromiseDeferred<void>&&);
    void setPaused(bool, DOMPromiseDeferred<void>&&);
    double currentTime() const;
    void setCurrentTime(double);
#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    bool canSetEntityTransform() const;
#endif
#if ENABLE(MODEL_ELEMENT_STAGE_MODE_INTERACTION)
    WEBCORE_EXPORT bool supportsStageModeInteraction() const;
    WEBCORE_EXPORT void beginStageModeTransform(const TransformationMatrix&);
    WEBCORE_EXPORT void updateStageModeTransform(const TransformationMatrix&);
    WEBCORE_EXPORT void endStageModeInteraction();
    WEBCORE_EXPORT void tryAnimateModelToFitPortal(bool handledDrag, CompletionHandler<void(bool)>&&);
    WEBCORE_EXPORT void resetModelTransformAfterDrag();
#endif

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
    ModelPlayerAccessibilityChildren accessibilityChildren();
#endif

    void sizeMayHaveChanged();

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    WEBCORE_EXPORT String inlinePreviewUUIDForTesting() const;
#endif

    size_t memoryCost() const;
#if ENABLE(RESOURCE_USAGE)
    size_t externalMemoryCost() const;
#endif

    bool isIntersectingViewport() const { return m_isIntersectingViewport; }
    void viewportIntersectionChanged(bool isIntersecting);

    WEBCORE_EXPORT String modelElementStateForTesting() const;

private:
    HTMLModelElement(const QualifiedName&, Document&);

    URL selectModelSource() const;
    void setSourceURL(const URL&);
    void modelDidChange();
    void createModelPlayer();
    void deleteModelPlayer();
    void unloadModelPlayer(bool onSuspend);
    void reloadModelPlayer();
    void startLoadModelTimer();
    void loadModelTimerFired();

    HTMLModelElement& readyPromiseResolve();

    CachedResourceRequest createResourceRequest(const URL&, FetchOptions::Destination);

    // ActiveDOMObject.
    bool virtualHasPendingActivity() const final;
    void resume() final;
    void suspend(ReasonForSuspension) final;
    void stop() final;

    // DOM overrides.
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;
    bool isURLAttribute(const Attribute&) const final;
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    // StyledElement
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;

    // Rendering overrides.
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool isReplaced(const RenderStyle* = nullptr) const final { return true; }
    void didAttachRenderers() final;

    // CachedRawResourceClient overrides.
    void dataReceived(CachedResource&, const SharedBuffer&) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;

    // ModelPlayerClient overrides.
    void didFinishLoading(ModelPlayer&) final;
    void didFailLoading(ModelPlayer&, const ResourceError&) final;
    void didUnload(ModelPlayer&) final;
    void didUpdate(ModelPlayer&) final;
#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    void didUpdateEntityTransform(ModelPlayer&, const TransformationMatrix&) final;
#endif
#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    void didUpdateBoundingBox(ModelPlayer&, const FloatPoint3D&, const FloatPoint3D&) final;
#endif
#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    void didFinishEnvironmentMapLoading(ModelPlayer&, bool succeeded) final;
#endif
    RefPtr<GraphicsLayer> graphicsLayer() const final;
    bool isVisible() const final;
    void logWarning(ModelPlayer&, const String&) final;

    Node::InsertedIntoAncestorResult insertedIntoAncestor(InsertionType , ContainerNode& parentOfInsertedTree) override;
    void removedFromAncestor(RemovalType, ContainerNode& oldParentOfRemovedTree) override;

    void defaultEventHandler(Event&) final;
    void dragDidStart(MouseEvent&);
    void dragDidChange(MouseEvent&);
    void dragDidEnd(MouseEvent&);

    LayoutPoint flippedLocationInElementForMouseEvent(MouseEvent&);

    void setAnimationIsPlaying(bool, DOMPromiseDeferred<void>&&);

    LayoutSize contentSize() const;
    bool modelContainerSizeIsEmpty() const;

    void reportExtraMemoryCost();

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)
    bool autoplay() const;
    void updateAutoplay();
    bool loop() const;
    void updateLoop();
#endif

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    void updateEnvironmentMap();
    URL selectEnvironmentMapURL() const;
    void environmentMapRequestResource();
    void environmentMapResetAndReject(Exception&&);
    void environmentMapResourceFinished();
#endif

#if ENABLE(MODEL_ELEMENT_PORTAL)
    bool hasPortal() const;
    void updateHasPortal();
#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    WebCore::StageModeOperation stageMode() const;
    void updateStageMode();
#endif

    void modelResourceFinished();
    void sourceRequestResource();
    bool shouldDeferLoading() const;
    bool isModelDeferred() const;
    bool isModelLoading() const;
    bool isModelLoaded() const;
    bool isModelUnloading() const;
    bool isModelUnloaded() const;

    URL m_sourceURL;
    CachedResourceHandle<CachedRawResource> m_resource;
    SharedBufferBuilder m_data;
    mutable std::atomic<size_t> m_dataMemoryCost { 0 };
    size_t m_reportedDataMemoryCost { 0 };
    WeakPtr<ModelPlayerProvider> m_modelPlayerProvider;
    RefPtr<Model> m_model;
    UniqueRef<ReadyPromise> m_readyPromise;
    bool m_dataComplete { false };
    bool m_isDragging { false };
    bool m_shouldCreateModelPlayerUponRendererAttachment { false };
    bool m_isIntersectingViewport { false };

    RefPtr<ModelPlayer> m_modelPlayer;
    EventLoopTimerHandle m_loadModelTimer;

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    Ref<DOMMatrixReadOnly> m_entityTransform;
#endif

#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    Ref<DOMPointReadOnly> m_boundingBoxCenter;
    Ref<DOMPointReadOnly> m_boundingBoxExtents;
#endif

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)
    double m_playbackRate { 1.0 };
#endif

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    URL m_environmentMapURL;
    SharedBufferBuilder m_environmentMapData;
    mutable std::atomic<size_t> m_environmentMapDataMemoryCost { 0 };

    CachedResourceHandle<CachedRawResource> m_environmentMapResource;
    UniqueRef<EnvironmentMapPromise> m_environmentMapReadyPromise;
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    bool m_detachedForImmersive { false };
    void setDetachedForImmersive(bool);

    Vector<CompletionHandler<void(ExceptionOr<RefPtr<ModelPlayer>>)>> m_modelPlayerCreationCallbacks;
    void ensureModelPlayer(CompletionHandler<void(ExceptionOr<RefPtr<ModelPlayer>>)>&&);
#endif

    void triggerModelPlayerCreationCallbacksIfNeeded(ExceptionOr<RefPtr<ModelPlayer>>&&);
};

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
