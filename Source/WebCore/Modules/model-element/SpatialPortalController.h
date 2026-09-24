/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(SPATIAL_PORTAL)

#include <WebCore/LayoutSize.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/NodeIdentifier.h>
#include <WebCore/PortalAction.h>
#include <WebCore/PortalTransform.h>
#include <WebCore/ResolvedScopedName.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Color;
class Element;
class EnvironmentMapLoader;
class GraphicsLayer;
class HTMLModelElement;
class IntersectionObserver;
class Model;
class ModelPlayer;
class ModelPlayerProvider;
class Page;
class PlaceholderModelPlayer;
class PortalModelPlayerClient;
class PortalVisibilityChangeClient;
class RenderBox;
class ResourceError;
class SharedBuffer;
class SpatialPortalEventListener;
class WeakPtrImplWithEventTargetData;

namespace Style {
class ComputedStyle;
struct ScopedName;
}

// Manages the portal / ModelPlayer for an element with `spatial: portal`.
class SpatialPortalController : public CanMakeWeakPtr<SpatialPortalController>, public CanMakeCheckedPtr<SpatialPortalController> {
    WTF_MAKE_TZONE_ALLOCATED(SpatialPortalController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SpatialPortalController);
    friend class PortalModelPlayerClient;
    friend class PortalIntersectionObserverCallback;
    friend class PortalVisibilityChangeClient;
public:
    explicit SpatialPortalController(Element&);
    ~SpatialPortalController();

    void prepareForRemoval();

    void unregisterChildModel(HTMLModelElement&);
    void registerChildModel(HTMLModelElement&);
    void childModelDidChange(HTMLModelElement&);
    void childVisibilityStateChanged(HTMLModelElement&);
    void childWasSuspended(HTMLModelElement&);
    void childTransformDidChange(HTMLModelElement&, const TransformationMatrix&);
    void scheduleAnchorUpdate();

    Element* portalElement() const { return m_portalElement.get(); }
    unsigned numberOfHostedModels() const { return m_hostedModels.size(); }
    bool childIsLoaded(NodeIdentifier) const;
    ModelPlayer* playerForChild(NodeIdentifier) const;
    WEBCORE_EXPORT HTMLModelElement* anchorModelForChild(NodeIdentifier) const;
    void configureGraphicsLayer(GraphicsLayer&, const Color& backgroundColor);
    void sizeMayHaveChanged();

    void updatePortalTransform(const RenderBox&);
    void updatePortalTransform();
    const std::optional<TransformationMatrix>& resolvedPortalTransform() const { return m_resolvedPortalTransform; }

    void setPortalAction(PortalActionKind);

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    void environmentMapStyleDidChange();
    WEBCORE_EXPORT String effectiveEnvironmentMapForTesting() const;
#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE_INTERACTION)
    WEBCORE_EXPORT static CheckedPtr<SpatialPortalController> interactiveControllerForHitTestedElement(Element*);
    WEBCORE_EXPORT bool supportsInteraction() const;
    WEBCORE_EXPORT void beginStageModeTransform(const TransformationMatrix&);
    WEBCORE_EXPORT void updateStageModeTransform(const TransformationMatrix&);
    WEBCORE_EXPORT void endStageModeInteraction();
#endif

    bool isPortalVisible() const;

private:
    struct HostedModel {
        WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> element;
        RefPtr<Model> loadedModel;
        RefPtr<PlaceholderModelPlayer> placeholder;

        std::optional<NodeIdentifier> anchorNode;
        String anchorPlacement;
        bool anchorWarningIssued { false };
    };

    struct AnchorResolution {
        std::optional<NodeIdentifier> node;
        String warning;
    };

    using AnchorsByName = HashMap<Style::ResolvedScopedName, NodeIdentifier>;

    void modelDidFinishLoading(ModelPlayer&, NodeIdentifier);
    void modelDidFailLoading(ModelPlayer&, NodeIdentifier, const ResourceError&);
    void modelDidUnload(ModelPlayer&);
    void modelDidUpdate(ModelPlayer&);
    void modelDidUpdatePortalTransform(ModelPlayer&, const TransformationMatrix&);
    void logWarning(ModelPlayer&, const String&);
    RefPtr<GraphicsLayer> portalGraphicsLayer() const;
    void viewportIntersectionChanged(bool isIntersecting);
    void documentVisibilityChanged();

    ModelPlayer* ensureModelPlayer();
    void loadChildModelsIfReady();
    void loadChildModelIfReady(HTMLModelElement&);
    void deleteModelPlayer();
    void unloadChildModel(NodeIdentifier);
    void unloadAllChildModels();
    void saveChildState(NodeIdentifier, HostedModel&, bool onSuspend);
    HTMLModelElement* hostedModelElement(NodeIdentifier) const;
    void updateAnchors();
    AnchorsByName collectAnchorNames() const;
    AnchorResolution resolvedAnchorNode(const HTMLModelElement&, const Style::ComputedStyle&, const AnchorsByName&) const;
    void updateAnchorForChild(const HTMLModelElement&, HostedModel&, const Style::ComputedStyle&, const AnchorsByName&);
    static std::optional<NodeIdentifier> anchorNodeForName(const HTMLModelElement&, const Style::ScopedName&, const AnchorsByName&);
    bool anchorChainReaches(NodeIdentifier startNode, NodeIdentifier targetNode, const AnchorsByName&) const;
    void addConsoleWarning(const String&) const;
    void reconfigurePortalLayer();
    void observePortalVisibility();
    void stopObservingPortalVisibility();
    LayoutSize portalContentSize() const;
    void updateGestureHandling();

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)

    void pushEnvironmentMapToPlayer(ModelPlayer&) const;
    void updateEnvironmentMap();
    void startEnvironmentMapLoad();
    void environmentMapDidLoad(const URL&, RefPtr<SharedBuffer>&&);
#endif

    const WeakPtr<Element, WeakPtrImplWithEventTargetData> m_portalElement;

    HashMap<NodeIdentifier, HostedModel> m_hostedModels;

    WeakPtr<ModelPlayerProvider> m_modelPlayerProvider;
#if ENABLE(MODEL_PROCESS)
    WeakPtr<Page> m_page;
#endif
    RefPtr<ModelPlayer> m_modelPlayer;
    const RefPtr<PortalModelPlayerClient> m_playerClient;
    RefPtr<IntersectionObserver> m_intersectionObserver;
    RefPtr<PortalVisibilityChangeClient> m_visibilityChangeClient;
#if ENABLE(TOUCH_EVENTS)
    RefPtr<SpatialPortalEventListener> m_eventListener;
#endif
    std::optional<LayoutSize> m_lastPushedContentSize;
    std::optional<TransformationMatrix> m_resolvedPortalTransform;
    UsedPortalTransform m_portalTransform;
    PortalActionKind m_portalAction { PortalActionKind::None };
#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    URL m_environmentMapURL;
    RefPtr<SharedBuffer> m_environmentMapData;
    RefPtr<EnvironmentMapLoader> m_environmentMapLoader;
    EnvironmentMapKind m_environmentMapKind { EnvironmentMapKind::Default };
    bool m_environmentMapFailed { false };
#endif
    bool m_anchorUpdateScheduled { false };
    bool m_handlesGesture { false };
    bool m_isIntersectingViewport { false };
};

} // namespace WebCore

#endif // ENABLE(SPATIAL_PORTAL)
