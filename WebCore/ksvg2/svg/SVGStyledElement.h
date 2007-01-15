/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGStyledElement_H
#define SVGStyledElement_H

#ifdef SVG_SUPPORT

#include "AffineTransform.h"
#include "Path.h"
#include "SVGElement.h"
#include "SVGLength.h"
#include "SVGResource.h"
#include "SVGStylable.h"

namespace WebCore {

    class CSSStyleDeclaration;
    class RenderPath;
    class RenderView;

    class SVGStyledElement : public SVGElement {
    public:
        SVGStyledElement(const QualifiedName&, Document*);
        virtual ~SVGStyledElement();
        
        virtual bool isStyled() const { return true; }

        // 'SVGStylable' functions
        // These need to be implemented.
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual Path toPathData() const { return Path(); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource() { return 0; }
        
        virtual void parseMappedAttribute(MappedAttribute*);

        RenderView* view() const;

        virtual void notifyAttributeChange() const;
        virtual void attributeChanged(Attribute*, bool preserveDecls = false);

    protected:
        friend class RenderPath;
        void rebuildRenderer() const;

        virtual bool hasRelativeValues() const { return false; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGStyledElement, String, String, ClassName, className)
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // SVGStyledElement

// vim:ts=4:noet
