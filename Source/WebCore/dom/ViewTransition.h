/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "ActiveDOMObject.h"
#include "Document.h"
#include "Element.h"
#include "ExceptionOr.h"
#include "ImageBuffer.h"
#include "MutableStyleProperties.h"
#include "Styleable.h"
#include "ViewTransitionUpdateCallback.h"
#include "VisibilityChangeClient.h"
#include <wtf/CheckedRef.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomString.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class DOMPromise;
class DeferredPromise;
class RenderLayerModelObject;
class RenderViewTransitionCapture;
class RenderLayerModelObject;
class ViewTransitionTypeSet;

enum class ViewTransitionPhase : uint8_t {
    PendingCapture,
    CapturingOldState, // Not part of the spec.
    UpdateCallbackCalled,
    Animating,
    Done
};

struct CapturedElement {
    WTF_MAKE_TZONE_ALLOCATED(CapturedElement);
public:
    // std::nullopt represents an non-capturable element.
    // nullptr represents an absent snapshot on an capturable element.
    std::optional<RefPtr<ImageBuffer>> oldImage;
    LayoutRect oldOverflowRect;
    LayoutPoint oldLayerToLayoutOffset;
    LayoutSize oldSize;
    RefPtr<MutableStyleProperties> oldProperties;
    bool initiallyIntersectsViewport { false };

    WeakStyleable newElement;
    LayoutRect newOverflowRect;
    LayoutSize newSize;

    Vector<AtomString> classList;
    RefPtr<MutableStyleProperties> groupStyleProperties;
};

struct OrderedNamedElementsMap {
public:
    bool contains(const AtomString& key) const
    {
        return m_keys.contains(key);
    }

    void add(const AtomString& key, CapturedElement& value)
    {
        m_keys.add(key);
        m_map.set(key, makeUniqueRef<CapturedElement>(value));
    }

    void remove(const AtomString& key)
    {
        m_map.remove(key);
        m_keys.remove(key);
    }

    auto& keys() const
    {
        return m_keys;
    }

    auto& map() const
    {
        return m_map;
    }

    auto& map()
    {
        return m_map;
    }

    bool isEmpty() const
    {
        return m_keys.isEmpty();
    }

    size_t size() const
    {
        return m_keys.size();
    }

    CapturedElement* find(const AtomString& key)
    {
        if (auto it = m_map.find(key); it != m_map.end())
            return &it->value;
        return nullptr;
    }

    const CapturedElement* find(const AtomString& key) const
    {
        if (auto it = m_map.find(key); it != m_map.end())
            return &it->value;
        return nullptr;
    }

    void swap(OrderedNamedElementsMap& other)
    {
        m_keys.swap(other.m_keys);
        m_map.swap(other.m_map);
    }

private:
    ListHashSet<AtomString> m_keys;
    HashMap<AtomString, UniqueRef<CapturedElement>> m_map;
};

struct ViewTransitionParams {
    WTF_MAKE_TZONE_ALLOCATED(ViewTransitionParams);
public:

    OrderedNamedElementsMap namedElements;
    FloatSize initialLargeViewportSize;
    float initialPageZoom;
    MonotonicTime startTime;
};

class ViewTransition : public RefCounted<ViewTransition>, public VisibilityChangeClient, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED(ViewTransition);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<ViewTransition> createSamePage(Document&, RefPtr<ViewTransitionUpdateCallback>&&, Vector<AtomString>&&);
    static RefPtr<ViewTransition> resolveInboundCrossDocumentViewTransition(Document&, std::unique_ptr<ViewTransitionParams>);
    static Ref<ViewTransition> setupCrossDocumentViewTransition(Document&);
    ~ViewTransition();

    void skipTransition();
    void skipViewTransition(ExceptionOr<JSC::JSValue>&&);

    void setupViewTransition();
    void handleTransitionFrame();

    void activateViewTransition();

    UniqueRef<ViewTransitionParams> takeViewTransitionParams();

    DOMPromise& ready();
    DOMPromise& updateCallbackDone();
    DOMPromise& finished();

    ViewTransitionPhase phase() const { return m_phase; }
    const OrderedNamedElementsMap& namedElements() const { return m_namedElements; };

    Document* document() const { return downcast<Document>(scriptExecutionContext()); }
    RefPtr<Document> protectedDocument() const { return document(); }

    bool documentElementIsCaptured() const;

    const ViewTransitionTypeSet& types() const { return m_types; }
    void setTypes(Ref<ViewTransitionTypeSet>&&);

    RenderViewTransitionCapture* viewTransitionNewPseudoForCapturedElement(RenderLayerModelObject&);

    static constexpr Seconds defaultTimeout = 4_s;

private:
    ViewTransition(Document&, RefPtr<ViewTransitionUpdateCallback>&&, Vector<AtomString>&&);
    ViewTransition(Document&, Vector<AtomString>&&);

    Ref<MutableStyleProperties> copyElementBaseProperties(RenderLayerModelObject&, LayoutSize&, LayoutRect& overflowRect, bool& intersectsViewport);
    bool updatePropertiesForRenderer(CapturedElement&, RenderBoxModelObject*, const AtomString&);

    // Setup view transition sub-algorithms.
    ExceptionOr<void> captureOldState();
    ExceptionOr<void> captureNewState();
    void setupTransitionPseudoElements();
    void setupDynamicStyleSheet(const AtomString&, const CapturedElement&);

    void callUpdateCallback();

    void updatePseudoElementStyles();
    ExceptionOr<void> updatePseudoElementSizes();
    ExceptionOr<void> checkForViewportSizeChange();

    void clearViewTransition();

    // VisibilityChangeClient.
    void visibilityStateChanged() final;

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    bool isCrossDocument() { return m_isCrossDocument; }

    OrderedNamedElementsMap m_namedElements;
    ViewTransitionPhase m_phase { ViewTransitionPhase::PendingCapture };
    FloatSize m_initialLargeViewportSize;
    float m_initialPageZoom;

    RefPtr<ViewTransitionUpdateCallback>  m_updateCallback;
    bool m_isCrossDocument { false };

    using PromiseAndWrapper = std::pair<Ref<DOMPromise>, Ref<DeferredPromise>>;
    PromiseAndWrapper m_ready;
    PromiseAndWrapper m_updateCallbackDone;
    PromiseAndWrapper m_finished;
    EventLoopTimerHandle m_updateCallbackTimeout;

    Ref<ViewTransitionTypeSet> m_types;
};

}
