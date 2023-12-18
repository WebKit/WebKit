/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "CachedResourceHandle.h"
#include "CachedSVGDocumentClient.h"
#include "SVGGraphicsElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class CachedSVGDocument;

class SVGUseElement final : public SVGGraphicsElement, public SVGURIReference, private CachedSVGDocumentClient {
    WTF_MAKE_ISO_ALLOCATED(SVGUseElement);
public:
    static Ref<SVGUseElement> create(const QualifiedName&, Document&);
    virtual ~SVGUseElement();

    void invalidateShadowTree();
    void updateUserAgentShadowTree() final;

    RefPtr<SVGElement> clipChild() const;
    RenderElement* rendererClipChild() const;

    const SVGLengthValue& x() const { return m_x->currentValue(); }
    const SVGLengthValue& y() const { return m_y->currentValue(); }
    const SVGLengthValue& width() const { return m_width->currentValue(); }
    const SVGLengthValue& height() const { return m_height->currentValue(); }

    SVGAnimatedLength& xAnimated() { return m_x; }
    SVGAnimatedLength& yAnimated() { return m_y; }
    SVGAnimatedLength& widthAnimated() { return m_width; }
    SVGAnimatedLength& heightAnimated() { return m_height; }

private:
    SVGUseElement(const QualifiedName&, Document&);

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void didFinishInsertingNode() final;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void buildPendingResource() override;

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGUseElement, SVGGraphicsElement, SVGURIReference>;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    Path toClipPath() override;
    bool selfHasRelativeLengths() const override;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;

    Document* externalDocument() const;
    void updateExternalDocument();

    SVGElement* findTarget(AtomString* targetID = nullptr) const;

    void cloneTarget(ContainerNode&, SVGElement& target) const;
    RefPtr<SVGElement> targetClone() const;

    void expandUseElementsInShadowTree() const;
    void expandSymbolElementsInShadowTree() const;
    void transferEventListenersToShadowTree() const;
    void transferSizeAttributesToTargetClone(SVGElement&) const;

    void clearShadowTree();
    void invalidateDependentShadowTrees();

    bool haveLoadedRequiredResources() override { return SVGURIReference::haveLoadedRequiredResources(); }
    void setHaveFiredLoadEvent(bool haveFiredLoadEvent) override { m_haveFiredLoadEvent = haveFiredLoadEvent; }
    bool haveFiredLoadEvent() const override { return m_haveFiredLoadEvent; }
    void setErrorOccurred(bool errorOccurred) override { m_errorOccurred = errorOccurred; }
    bool errorOccurred() const override { return m_errorOccurred; }
    Timer* loadEventTimer() override { return &m_loadEventTimer; }

    bool isValid() const override { return SVGTests::isValid(); }

    Ref<SVGAnimatedLength> m_x { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_y { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    Ref<SVGAnimatedLength> m_width { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_height { SVGAnimatedLength::create(this, SVGLengthMode::Height) };

    bool m_haveFiredLoadEvent { false };
    bool m_errorOccurred { false };
    bool m_shadowTreeNeedsUpdate { true };
    CachedResourceHandle<CachedSVGDocument> m_externalDocument;
    Timer m_loadEventTimer;
};

}
