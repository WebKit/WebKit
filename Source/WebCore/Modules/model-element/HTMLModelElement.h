/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_ELEMENT)

#include "ActiveDOMObject.h"
#include "CachedRawResource.h"
#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "HTMLElement.h"
#include "HTMLModelElementCamera.h"
#include "IDLTypes.h"
#include "ModelPlayerClient.h"
#include "PlatformLayer.h"
#include "PlatformLayerIdentifier.h"
#include "SharedBuffer.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class Event;
class LayoutSize;
class Model;
class ModelPlayer;
class MouseEvent;

template<typename IDLType> class DOMPromiseDeferred;
template<typename IDLType> class DOMPromiseProxyWithResolveCallback;

class HTMLModelElement final : public HTMLElement, private CachedRawResourceClient, public ModelPlayerClient, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(HTMLModelElement);
public:
    using HTMLElement::weakPtrFactory;
    using HTMLElement::WeakValueType;
    using HTMLElement::WeakPtrImplType;

    static Ref<HTMLModelElement> create(const QualifiedName&, Document&);
    virtual ~HTMLModelElement();

    void sourcesChanged();
    const URL& currentSrc() const { return m_sourceURL; }
    bool complete() const { return m_dataComplete; }

    // MARK: DOM Functions and Attributes

    using ReadyPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<HTMLModelElement>>;
    ReadyPromise& ready() { return m_readyPromise.get(); }

    RefPtr<Model> model() const;

    bool usesPlatformLayer() const;
    PlatformLayer* platformLayer() const;

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

    bool supportsDragging() const;
    bool isDraggableIgnoringAttributes() const final;

    bool isInteractive() const;

#if PLATFORM(COCOA)
    Vector<RetainPtr<id>> accessibilityChildren();
#endif

    void sizeMayHaveChanged();

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    WEBCORE_EXPORT String inlinePreviewUUIDForTesting() const;
#endif

private:
    HTMLModelElement(const QualifiedName&, Document&);

    URL selectModelSource() const;
    void setSourceURL(const URL&);
    void modelDidChange();
    void createModelPlayer();

    HTMLModelElement& readyPromiseResolve();

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    // DOM overrides.
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;
    bool isURLAttribute(const Attribute&) const final;
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    // StyledElement
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;

    // Rendering overrides.
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void didAttachRenderers() final;

    // CachedRawResourceClient overrides.
    void dataReceived(CachedResource&, const SharedBuffer&) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;

    // ModelPlayerClient overrides.
    void didFinishLoading(ModelPlayer&) final;
    void didFailLoading(ModelPlayer&, const ResourceError&) final;
    PlatformLayerIdentifier platformLayerID() final;

    void defaultEventHandler(Event&) final;
    void dragDidStart(MouseEvent&);
    void dragDidChange(MouseEvent&);
    void dragDidEnd(MouseEvent&);

    LayoutPoint flippedLocationInElementForMouseEvent(MouseEvent&);

    void setAnimationIsPlaying(bool, DOMPromiseDeferred<void>&&);

    LayoutSize contentSize() const;

    URL m_sourceURL;
    CachedResourceHandle<CachedRawResource> m_resource;
    SharedBufferBuilder m_data;
    RefPtr<Model> m_model;
    UniqueRef<ReadyPromise> m_readyPromise;
    bool m_dataComplete { false };
    bool m_isDragging { false };
    bool m_shouldCreateModelPlayerUponRendererAttachment { false };

    RefPtr<ModelPlayer> m_modelPlayer;
};

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
