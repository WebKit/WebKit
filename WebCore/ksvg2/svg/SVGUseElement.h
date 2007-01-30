/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef SVGUseElement_h
#define SVGUseElement_h

#ifdef SVG_SUPPORT

#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore
{
    class SVGElementInstance;
    class SVGLength;

    class SVGUseElement : public SVGStyledTransformableElement,
                          public SVGTests,
                          public SVGLangSpace,
                          public SVGExternalResourcesRequired,
                          public SVGURIReference
    {
    public:
        SVGUseElement(const QualifiedName&, Document*);
        virtual ~SVGUseElement();

        SVGElementInstance* instanceRoot() const;
        SVGElementInstance* animatedInstanceRoot() const;

        virtual bool isValid() const { return SVGTests::isValid(); }

        // Derived from: 'Element'
        virtual void insertedIntoDocument();
        virtual void removedFromDocument();
        virtual void buildPendingResource();

        // 'SVGUseElement' functions
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void notifyAttributeChange() const;

        virtual bool rendererIsNeeded(RenderStyle* style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual void attach();

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)

        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGLength, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGLength, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGLength, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGUseElement, SVGLength, SVGLength, Height, height)

    private:
        // Instance tree handling
        void buildInstanceTree(SVGElement* target, SVGElementInstance* targetInstance, bool& foundCycle);
        void handleDeepUseReferencing(SVGElement* use, SVGElementInstance* targetInstance, bool& foundCycle);

        // Shadow tree handling
        void buildShadowTree(SVGElement* target, SVGElementInstance* targetInstance);
        void expandUseElementsInShadowTree(Node* element);
        void attachShadowTree();

        // "Tree connector" 
        void associateInstancesWithShadowTreeElements(Node* target, SVGElementInstance* targetInstance);

        void transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const;

        RefPtr<SVGElement> m_shadowTreeRootElement;
        RefPtr<SVGElementInstance> m_targetElementInstance;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet

