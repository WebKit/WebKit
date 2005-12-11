/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGElementImpl_H
#define KSVG_SVGElementImpl_H

#include "SVGNames.h"
#include <kdom/core/XMLElementImpl.h>

namespace KDOM
{
    class Ecma;
    class DocumentPtr;
    typedef StyledElementImpl XMLElementImpl;
};

namespace KSVG
{
    class SVGMatrixImpl;
    class SVGSVGElementImpl;
    class SVGStyledElementImpl;

    class SVGElementImpl : public KDOM::XMLElementImpl
    {
    public:
        SVGElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGElementImpl();
        virtual bool isSVGElement() const { return true; }
        virtual bool isSupported(KDOM::DOMStringImpl *feature, KDOM::DOMStringImpl *version) const;

        SVGSVGElementImpl *ownerSVGElement() const;
        SVGElementImpl *viewportElement() const;

        // Helper methods that returns the attr value if attr is set, otherwise the default value.
        // It throws NO_MODIFICATION_ALLOWED_ERR if the element is read-only.
        KDOM::AtomicString tryGetAttribute(const KDOM::DOMString& name, KDOM::AtomicString defaultValue = KDOM::AtomicString()) const;
        KDOM::AtomicString tryGetAttributeNS(const KDOM::DOMString& namespaceURI, const KDOM::DOMString& localName, KDOM::AtomicString defaultValue = KDOM::AtomicString()) const;

        // Internal
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        // To be implemented by any element which can establish new viewports...
        virtual QString adjustViewportClipping() const { return QString::null; }
        
        virtual bool isStyled() const { return false; }
        virtual bool isStyledTransformable() const { return false; }
        virtual bool isStyledLocatable() const { return false; }
        virtual bool isSVG() const { return false; }
        virtual bool isFilterEffect() const { return false; }
        virtual bool isGradientStop() const { return false; }
        
        // For SVGTestsImpl
        virtual bool isValid() const { return true; }
        
        virtual void closeRenderer() { m_closed = true; }
        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return false; }
        virtual bool childShouldCreateRenderer(DOM::NodeImpl *) const;
        
        // helper:
        bool isClosed() const { return m_closed; }

    private:
        bool m_closed;
#if 0
        void addSVGEventListener(KDOM::Ecma *ecmaEngine, const KDOM::DOMString &type, const KDOM::DOMString &value);
#endif
    };
};

// Helper for DOMNode -> SVGElement conversion
#include <kdom/kdom.h>
#include <kdom/Namespace.h>
#include <kdom/DOMString.h>

namespace KSVG {
    static inline SVGElementImpl *svg_dynamic_cast(KDOM::NodeImpl *node) {
        SVGElementImpl *svgElement = NULL;
#if APPLE_CHANGES
        if (node && node->isSVGElement())
            svgElement = static_cast<SVGElementImpl *>(node);
#else
        if (node && (node->nodeType() == KDOM::ELEMENT_NODE)) {
            KDOM::ElementImpl *element = static_cast<KDOM::ElementImpl *>(node);
            if (element && (KDOM::DOMString(element->namespaceURI()) == KDOM::NS_SVG))
                svgElement = static_cast<SVGElementImpl *>(element);
        }
#endif
        return svgElement;
    }
};

#endif

// vim:ts=4:noet
