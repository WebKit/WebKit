/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGUseElement_h
#define SVGUseElement_h

#if ENABLE(SVG)
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore {

    class SVGElementInstance;
    class SVGLength;

    class SVGUseElement : public SVGStyledTransformableElement,
                          public SVGTests,
                          public SVGLangSpace,
                          public SVGExternalResourcesRequired,
                          public SVGURIReference {
    public:
        SVGUseElement(const QualifiedName&, Document*);
        virtual ~SVGUseElement();

        SVGElementInstance* instanceRoot() const;
        SVGElementInstance* animatedInstanceRoot() const;

        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void insertedIntoDocument();
        virtual void removedFromDocument();
        virtual void buildPendingResource();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void recalcStyle(StyleChange = NoChange);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual void attach();
        virtual void detach();

        virtual Path toClipPath() const;

        static void removeDisallowedElementsFromSubtree(Node* element);
        SVGElementInstance* instanceForShadowTreeElement(Node* element) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGNames::useTagString, SVGNames::xAttrString, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGNames::useTagString, SVGNames::yAttrString, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGNames::useTagString, SVGNames::widthAttrString, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGNames::useTagString, SVGNames::heightAttrString, SVGLength, Height, height)

        // SVGURIReference
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGURIReferenceIdentifier, XLinkNames::hrefAttrString, String, Href, href)

    private:
        // Instance tree handling
        void buildInstanceTree(SVGElement* target, SVGElementInstance* targetInstance, bool& foundCycle);
        void handleDeepUseReferencing(SVGUseElement* use, SVGElementInstance* targetInstance, bool& foundCycle);

        // Shadow tree handling
        PassRefPtr<SVGSVGElement> buildShadowTreeForSymbolTag(SVGElement* target, SVGElementInstance* targetInstance);
        void alterShadowTreeForSVGTag(SVGElement* target);

        void buildShadowTree(SVGElement* target, SVGElementInstance* targetInstance);

#if ENABLE(SVG) && ENABLE(SVG_USE)
        void expandUseElementsInShadowTree(Node* element);
        void expandSymbolElementsInShadowTree(Node* element);
#endif

        void attachShadowTree();

        // "Tree connector" 
        void associateInstancesWithShadowTreeElements(Node* target, SVGElementInstance* targetInstance);
        SVGElementInstance* instanceForShadowTreeElement(Node* element, SVGElementInstance* instance) const;

        void transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const;
        void transferEventListenersToShadowTree(SVGElementInstance* target);

        RefPtr<SVGElement> m_shadowTreeRootElement;
        RefPtr<SVGElementInstance> m_targetElementInstance;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
