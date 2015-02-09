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

#ifndef SVGUseElement_h
#define SVGUseElement_h

#include "CachedResourceHandle.h"
#include "CachedSVGDocumentClient.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGURIReference.h"

namespace WebCore {

class CachedSVGDocument;
class SVGGElement;

class SVGUseElement final : public SVGGraphicsElement,
                            public SVGExternalResourcesRequired,
                            public SVGURIReference,
                            public CachedSVGDocumentClient {
public:
    static Ref<SVGUseElement> create(const QualifiedName&, Document&, bool wasInsertedByParser);
    virtual ~SVGUseElement();

    void invalidateShadowTree();
    void invalidateDependentShadowTrees();

    RenderElement* rendererClipChild() const;

protected:
    virtual void didNotifySubtreeInsertions(ContainerNode*) override;

private:
    SVGUseElement(const QualifiedName&, Document&, bool wasInsertedByParser);

    virtual bool isValid() const override { return SVGTests::isValid(); }

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void buildPendingResource() override;

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void svgAttributeChanged(const QualifiedName&) override;

    virtual void willAttachRenderers() override;

    virtual RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&) override;
    virtual void toClipPath(Path&) override;

    void clearResourceReferences();

    virtual bool haveLoadedRequiredResources() override { return SVGExternalResourcesRequired::haveLoadedRequiredResources(); }

    virtual void finishParsingChildren() override;
    virtual bool selfHasRelativeLengths() const override;

    // Shadow tree handling.
    void buildShadowTree(SVGElement& target);
    void expandUseElementsInShadowTree();
    void expandSymbolElementsInShadowTree();
    SVGElement* shadowTreeTargetClone() const;
    void transferEventListenersToShadowTree();
    void transferAttributesToShadowTreeReplacement(SVGGElement&) const;
    void transferSizeAttributesToShadowTreeTargetClone(SVGElement&) const;
    bool isValidTarget(Element*) const;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGUseElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    bool cachedDocumentIsStillLoading();
    Document* externalDocument() const;
    virtual void notifyFinished(CachedResource*) override;
    Document* referencedDocument() const;
    void setCachedDocument(CachedResourceHandle<CachedSVGDocument>);

    // SVGExternalResourcesRequired
    virtual void setHaveFiredLoadEvent(bool haveFiredLoadEvent) override { m_haveFiredLoadEvent = haveFiredLoadEvent; }
    virtual bool isParserInserted() const override { return m_wasInsertedByParser; }
    virtual bool haveFiredLoadEvent() const override { return m_haveFiredLoadEvent; }
    virtual Timer* svgLoadEventTimer() override { return &m_svgLoadEventTimer; }

    bool m_wasInsertedByParser;
    bool m_haveFiredLoadEvent;
    bool m_needsShadowTreeRecreation;
    CachedResourceHandle<CachedSVGDocument> m_cachedDocument;
    Timer m_svgLoadEventTimer;
};

}

#endif
