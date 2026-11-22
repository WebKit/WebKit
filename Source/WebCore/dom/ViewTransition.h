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
#include "EventLoop.h"
#include "ImageBuffer.h"
#include "LayoutRect.h"
#include "MutableStyleProperties.h"
#include "NativeImage.h"
#include "Styleable.h"
#include "TransformationMatrix.h"
#include "ViewTransitionUpdateCallback.h"
#include "VisibilityChangeClient.h"
#include <wtf/CheckedRef.h>
#include <wtf/NativePromise.h>
#include <wtf/OrderedHashSet.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomString.h>

namespace JSC {
class JSValue;
}

namespace WTF {
class TextStream;
}

namespace WebCore {

class DOMPromise;
class DeferredPromise;
class GraphicsContext;
class ImageBuffer;
class RenderLayer;
class RenderBlockFlow;
class RenderBox;
class RenderLayerModelObject;
class ShadowRoot;
class RenderViewTransitionCapture;
class ViewTransitionTypeSet;
template<typename> class ExceptionOr;

namespace Style {
class RuleSet;
class Scope;
}

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
    struct State {
        LayoutRect overflowRect;
        LayoutPoint layerToLayoutOffset;
        LayoutSize size;
        LayoutSize subpixelOffset;
        RefPtr<MutableStyleProperties> properties;
        // Maps the element's border box to the snapshot containing block, used to
        // re-express nested group transforms relative to their containing group.
        std::optional<TransformationMatrix> snapshotMatrix;
        // Border widths offset nested group geometry: child transforms are
        // relative to this group's padding box edge.
        bool intersectsViewport { false };
        bool isRootElement { false };
        bool hasNonInvertibleTransform { false };
    };

    // std::nullopt represents an non-capturable element.
    // nullptr represents an absent snapshot on an capturable element.
    std::optional<RefPtr<ImageBuffer>> oldImage;
    // Instead of *oldImage when the capture includes a frame hosted in another process.
    RefPtr<NativeImage> oldDisplayOnlyImage;
    RefPtr<NativePromise<Ref<NativeImage>, void>> pendingCapture;
    State oldState;

    WeakStyleable newElement;
    State newState;

    Vector<AtomString> classList;
    RefPtr<MutableStyleProperties> groupStyleProperties;
    // A generated name derived from a shadow tree element must not leak its id;
    // script-facing serialization uses "match-element" instead.
    bool generatedNameFromShadowTree { false };
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
    OrderedHashSet<AtomString> m_keys;
    HashMap<AtomString, UniqueRef<CapturedElement>> m_map;
};

struct ViewTransitionParams {
    WTF_MAKE_TZONE_ALLOCATED(ViewTransitionParams);
public:

    OrderedNamedElementsMap namedElements;
    FloatSize initialLargeViewportSize;
    float initialPageZoom { 1 };
    MonotonicTime startTime;
    RefPtr<SecurityOrigin> oldDocumentOrigin;
};

class ViewTransition : public RefCounted<ViewTransition>, public VisibilityChangeClient, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED(ViewTransition);
public:
    static Ref<ViewTransition> createSamePage(Document&, Element* scopeElement, RefPtr<ViewTransitionUpdateCallback>&&, Vector<AtomString>&&, Vector<Ref<ShadowRoot>>&& = { });
    static RefPtr<ViewTransition> resolveInboundCrossDocumentViewTransition(Document&, std::unique_ptr<ViewTransitionParams>);
    // Given the old document's params once its state has been captured, or null if the transition was
    // skipped. Proceeds with the navigation, which must not happen before then.
    using OutboundPostCaptureSteps = CompletionHandler<void(std::unique_ptr<ViewTransitionParams>&&)>;
    static Ref<ViewTransition> setupCrossDocumentViewTransition(Document&, OutboundPostCaptureSteps&&);
    ~ViewTransition();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(VisibilityChangeClient);

    void skipTransition();
    void skipViewTransition(ExceptionOr<JSC::JSValue>&&);

    void setupViewTransition();
    void handleTransitionFrame();

    void activateViewTransition();

    LayoutRect containingBlockRect();

    // Paints the frozen scope snapshot in place of the scope's layer while the
    // update callback runs; returns false once rendering is no longer suppressed.
    bool paintFrozenScopeSnapshot(GraphicsContext&, RenderLayer&, const RenderLayer* rootLayer, const LayoutSize& subpixelOffset);

    NODELETE DOMPromise& ready();
    NODELETE DOMPromise& updateCallbackDone();
    NODELETE DOMPromise& finished();

    ViewTransitionPhase phase() const { return m_phase; }
    const OrderedNamedElementsMap& namedElements() const { return m_namedElements; };

    NODELETE Document* document() const;

    NODELETE Element* scopeElement() const { return m_scopeElement.get(); }
    NODELETE Element* transitionRoot() const;
    NODELETE Style::Scope* transitionStyleScope() const;

    bool hasPseudoElementTree() const { return m_hasPseudoElementTree; }

    // Renderers of the pseudo element tree hosted on the scope element. The
    // document transition uses the equivalent storage on RenderView instead.
    NODELETE RenderBlockFlow* scopedViewTransitionContainingBlock() const;
    void setScopedViewTransitionContainingBlock(RenderBlockFlow&);
    void addScopedViewTransitionGroup(const AtomString&, RenderBox&);
    void removeScopedViewTransitionGroup(const AtomString&);
    NODELETE RenderBox* scopedViewTransitionGroupForName(const AtomString&) const;

    void reinstallDynamicStyles();

    // UA rules generated for this transition's pseudo-elements; matched only when
    // resolving pseudos hosted on this transition's scope, so concurrent scoped
    // transitions with equal participant names stay independent.
    NODELETE Style::RuleSet* dynamicStyleRules() const;

    NODELETE bool documentElementIsCaptured() const;

    const ViewTransitionTypeSet& types() const { return m_types; }
    void setTypes(Ref<ViewTransitionTypeSet>&&);

    RenderViewTransitionCapture* viewTransitionNewPseudoForCapturedElement(RenderLayerModelObject&);

    static constexpr Seconds defaultTimeout = 4_s;

private:
    ViewTransition(Document&, Element* scopeElement, RefPtr<ViewTransitionUpdateCallback>&&, Vector<AtomString>&&, Vector<Ref<ShadowRoot>>&&);
    ViewTransition(Document&, Vector<AtomString>&&);

    void copyElementBaseProperties(RenderLayerModelObject&, CapturedElement::State&);
    bool updatePropertiesForGroupPseudo(CapturedElement&, const AtomString&);
    LayoutRect captureOverflowRect(RenderLayerModelObject& renderer);

    // Setup view transition sub-algorithms.
    ExceptionOr<void> captureOldState();
    ExceptionOr<void> captureNewState();
    void setupTransitionPseudoElements();
    void setupDynamicStyleSheet(const AtomString&, const CapturedElement&);

    void callUpdateCallback();
    void scheduleUpdateCallback();
    static void flushUpdateCallbackQueue(Document&);

    ExceptionOr<void> updatePseudoElementStylesRead();
    void updatePseudoElementStylesWrite();
    ExceptionOr<void> updatePseudoElementRenderers();
    ExceptionOr<void> checkForViewportSizeChange();

    void clearViewTransition();
    bool isActiveTransition() const;
    bool isInTransitionScope(Element&) const;

    void whenPendingCapturesComplete(Function<void()>&&);
    void didCompletePendingCapture(const AtomString& name, RefPtr<NativeImage>&&);
    void stopWaitingForPendingCaptures();
    bool hasPendingCaptures() const;
    UniqueRef<ViewTransitionParams> takeViewTransitionParams();
    void runOutboundPostCaptureSteps();

    // VisibilityChangeClient.
    void visibilityStateChanged() final;

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    bool isCrossDocument() { return m_isCrossDocument; }

    OrderedNamedElementsMap m_namedElements;
    ViewTransitionPhase m_phase { ViewTransitionPhase::PendingCapture };
    FloatSize m_initialLargeViewportSize;
    float m_initialPageZoom { 1 };

    const RefPtr<ViewTransitionUpdateCallback> m_updateCallback;
    // Null for a document-scoped transition.
    const RefPtr<Element> m_scopeElement;
    const Vector<Ref<ShadowRoot>> m_includedShadowRoots;
    bool m_isCrossDocument { false };
    bool m_hasPseudoElementTree { false };

    void freezeScopeRendering();
    void unfreezeScopeRendering();

    Ref<Style::RuleSet> ensureDynamicStyleRules();
    AtomString groupKeyframesName(const AtomString&) const;

    RefPtr<Style::RuleSet> m_dynamicStyleRules;

    RefPtr<ImageBuffer> m_frozenScopeSnapshot;
    LayoutRect m_frozenScopeSnapshotRect;

    SingleThreadWeakPtr<RenderBlockFlow> m_scopedContainingBlockRenderer;
    HashMap<AtomString, SingleThreadWeakPtr<RenderBox>> m_scopedGroupRenderers;

    EventLoopTimerHandle m_pendingCapturesTimeout;
    Function<void()> m_whenPendingCapturesComplete;

    using PromiseAndWrapper = std::pair<Ref<DOMPromise>, Ref<DeferredPromise>>;
    PromiseAndWrapper m_ready;
    PromiseAndWrapper m_updateCallbackDone;
    PromiseAndWrapper m_finished;
    EventLoopTimerHandle m_updateCallbackTimeout;

    // Set on the old document's transition while a cross-document navigation waits for it to capture.
    OutboundPostCaptureSteps m_outboundPostCaptureSteps;

    Ref<ViewTransitionTypeSet> m_types;
};

WTF::TextStream& operator<<(WTF::TextStream&, ViewTransitionPhase);

} // namespace WebCore
