/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class CachedSVGDocument;
class SVGGElement;

class SVGUseElement final : public SVGGraphicsElement, public SVGExternalResourcesRequired, public SVGURIReference, private CachedSVGDocumentClient {
    WTF_MAKE_ISO_ALLOCATED(SVGUseElement);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGUseElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

public:
    static Ref<SVGUseElement> create(const QualifiedName&, Document&);
    virtual ~SVGUseElement();

    void invalidateShadowTree();
    bool shadowTreeNeedsUpdate() const { return m_shadowTreeNeedsUpdate; }
    void updateShadowTree();

    RenderElement* rendererClipChild() const;

private:
    SVGUseElement(const QualifiedName&, Document&);

    bool isValid() const override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void buildPendingResource() override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
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

    bool m_haveFiredLoadEvent { false };
    bool m_shadowTreeNeedsUpdate { true };
    CachedResourceHandle<CachedSVGDocument> m_externalDocument;
    Timer m_svgLoadEventTimer;
};

}
