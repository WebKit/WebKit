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

#include "LayoutSize.h"
#include <WebCore/NodeIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Color;
class Element;
class GraphicsLayer;
class HTMLModelElement;
class IntersectionObserver;
class Model;
class ModelPlayer;
class ModelPlayerProvider;
class PortalModelPlayerClient;
class ResourceError;
class WeakPtrImplWithEventTargetData;

// Manages the portal / ModelPlayer for an element with `spatial: portal`.
class SpatialPortalController : public CanMakeWeakPtr<SpatialPortalController>, public CanMakeCheckedPtr<SpatialPortalController> {
    WTF_MAKE_TZONE_ALLOCATED(SpatialPortalController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SpatialPortalController);
    friend class PortalModelPlayerClient;
    friend class PortalIntersectionObserverCallback;
public:
    explicit SpatialPortalController(Element&);
    ~SpatialPortalController();

    void unregisterChildModel(HTMLModelElement&);
    void registerChildModel(HTMLModelElement&);
    void childModelDidChange(HTMLModelElement&);

    ModelPlayer* modelPlayer() const { return m_modelPlayer.get(); }
    unsigned numberOfHostedModels() const { return m_model ? 1 : 0; }
    void configureGraphicsLayer(GraphicsLayer&, const Color& backgroundColor);
    void sizeMayHaveChanged();

    bool isPortalVisible() const;

private:
    void modelDidFinishLoading(ModelPlayer&, NodeIdentifier);
    void modelDidFailLoading(ModelPlayer&, NodeIdentifier, const ResourceError&);
    void modelDidUnload(ModelPlayer&);
    void modelDidUpdate(ModelPlayer&);
    void logWarning(ModelPlayer&, const String&);
    RefPtr<GraphicsLayer> portalGraphicsLayer() const;
    void viewportIntersectionChanged(bool isIntersecting);

    ModelPlayer* ensureModelPlayer();
    void loadChildModelIfReady();
    void unloadModel();
    void reconfigurePortalLayer();
    void observePortalVisibility();
    LayoutSize portalContentSize() const;

    const WeakPtr<Element, WeakPtrImplWithEventTargetData> m_portalElement;
    WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> m_childModel;
    WeakPtr<ModelPlayerProvider> m_modelPlayerProvider;
    RefPtr<ModelPlayer> m_modelPlayer;
    const RefPtr<PortalModelPlayerClient> m_playerClient;
    RefPtr<Model> m_model;
    RefPtr<IntersectionObserver> m_intersectionObserver;
    bool m_isIntersectingViewport { false };
};

} // namespace WebCore

#endif // ENABLE(SPATIAL_PORTAL)
