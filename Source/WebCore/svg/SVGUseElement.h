/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGElementInstance;
class SVGShadowTreeRootElement;

class SVGUseElement : public SVGStyledTransformableElement,
                      public SVGTests,
                      public SVGLangSpace,
                      public SVGExternalResourcesRequired,
                      public SVGURIReference {
public:
    static PassRefPtr<SVGUseElement> create(const QualifiedName&, Document*);

    SVGElementInstance* instanceRoot();
    SVGElementInstance* animatedInstanceRoot() const;
    SVGElementInstance* instanceForShadowTreeElement(Node*) const;
    void invalidateShadowTree();

    RenderObject* rendererClipChild() const;

private:
    SVGUseElement(const QualifiedName&, Document*);

    virtual bool isValid() const { return SVGTests::isValid(); }
    virtual bool supportsFocus() const { return true; }

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void buildPendingResource();

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(Attribute*) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool willRecalcStyle(StyleChange);
    virtual void didRecalcStyle(StyleChange);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void attach();
    virtual void detach();

    virtual void toClipPath(Path&);

    static void removeDisallowedElementsFromSubtree(Node* element);

    void setUpdatesBlocked(bool blocked) { m_updatesBlocked = blocked; }

    friend class RenderSVGShadowTreeRootContainer;
    friend class SVGElement;
    void buildShadowAndInstanceTree(SVGShadowTreeRootElement*);
    void detachInstance();

    virtual bool selfHasRelativeLengths() const;

    // Instance tree handling
    void buildInstanceTree(SVGElement* target, SVGElementInstance* targetInstance, bool& foundCycle);
    bool hasCycleUseReferencing(SVGUseElement*, SVGElementInstance* targetInstance, SVGElement*& newTarget);

    // Shadow tree handling
    void buildShadowTree(SVGShadowTreeRootElement*, SVGElement* target, SVGElementInstance* targetInstance);

    void expandUseElementsInShadowTree(Node* element);
    void expandSymbolElementsInShadowTree(Node* element);

    // "Tree connector" 
    void associateInstancesWithShadowTreeElements(Node* target, SVGElementInstance* targetInstance);
    SVGElementInstance* instanceForShadowTreeElement(Node* element, SVGElementInstance* instance) const;

    void transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const;
    void transferEventListenersToShadowTree(SVGElementInstance* target);

    void updateContainerOffsets();
    void updateContainerSizes();

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGUseElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    // SVGTests
    virtual void synchronizeRequiredFeatures() { SVGTests::synchronizeRequiredFeatures(this); }
    virtual void synchronizeRequiredExtensions() { SVGTests::synchronizeRequiredExtensions(this); }
    virtual void synchronizeSystemLanguage() { SVGTests::synchronizeSystemLanguage(this); }

    bool m_updatesBlocked;
    bool m_needsShadowTreeRecreation;
    String m_resourceId;
    RefPtr<SVGElementInstance> m_targetElementInstance;
};

}

#endif
#endif
