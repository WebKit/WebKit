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
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class CachedSVGDocument;
class SVGGElement;

class SVGUseElement final : public SVGGraphicsElement, public SVGExternalResourcesRequired, public SVGURIReference, private CachedSVGDocumentClient {
    WTF_MAKE_ISO_ALLOCATED(SVGUseElement);
public:
    static Ref<SVGUseElement> create(const QualifiedName&, Document&);
    virtual ~SVGUseElement();

    void invalidateShadowTree();
    bool shadowTreeNeedsUpdate() const { return m_shadowTreeNeedsUpdate; }
    void updateShadowTree();

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

    bool isValid() const override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void didFinishInsertingNode() final;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void buildPendingResource() override;

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGUseElement, SVGGraphicsElement, SVGExternalResourcesRequired, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    Path toClipPath() override;
    bool haveLoadedRequiredResources() override;
    void finishParsingChildren() override;
    bool selfHasRelativeLengths() const override;
    void setHaveFiredLoadEvent(bool) override;
    bool haveFiredLoadEvent() const override;
    Timer* svgLoadEventTimer() override;
    void notifyFinished(CachedResource&) final;

    Document* externalDocument() const;
    void updateExternalDocument();

    SVGElement* findTarget(String* targetID = nullptr) const;

    void cloneTarget(ContainerNode&, SVGElement& target) const;
    RefPtr<SVGElement> targetClone() const;

    void expandUseElementsInShadowTree() const;
    void expandSymbolElementsInShadowTree() const;
    void transferEventListenersToShadowTree() const;
    void transferSizeAttributesToTargetClone(SVGElement&) const;

    void clearShadowTree();
    void invalidateDependentShadowTrees();

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedLength> m_x { SVGAnimatedLength::create(this, LengthModeWidth) };
    Ref<SVGAnimatedLength> m_y { SVGAnimatedLength::create(this, LengthModeHeight) };
    Ref<SVGAnimatedLength> m_width { SVGAnimatedLength::create(this, LengthModeWidth) };
    Ref<SVGAnimatedLength> m_height { SVGAnimatedLength::create(this, LengthModeHeight) };

    bool m_haveFiredLoadEvent { false };
    bool m_shadowTreeNeedsUpdate { true };
    CachedResourceHandle<CachedSVGDocument> m_externalDocument;
    Timer m_svgLoadEventTimer;
};

}
