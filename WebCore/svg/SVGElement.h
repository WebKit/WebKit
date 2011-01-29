/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2009 Apple Inc. All rights reserved.

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

#ifndef SVGElement_h
#define SVGElement_h

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"
#include "StyledElement.h"

namespace WebCore {

    class CSSCursorImageValue;
    class Document;
    class SVGCursorElement;
    class SVGElementInstance;
    class SVGElementRareData;
    class SVGSVGElement;
    class AffineTransform;

    class SVGElement : public StyledElement {
    public:
        static PassRefPtr<SVGElement> create(const QualifiedName&, Document*);
        virtual ~SVGElement();

        String xmlbase() const;
        void setXmlbase(const String&, ExceptionCode&);

        SVGSVGElement* ownerSVGElement() const;
        SVGElement* viewportElement() const;

        SVGDocumentExtensions* accessDocumentSVGExtensions() const;

        virtual void parseMappedAttribute(MappedAttribute*);

        virtual bool isStyled() const { return false; }
        virtual bool isStyledTransformable() const { return false; }
        virtual bool isStyledLocatable() const { return false; }
        virtual bool isSVG() const { return false; }
        virtual bool isFilterEffect() const { return false; }
        virtual bool isGradientStop() const { return false; }
        virtual bool isTextContent() const { return false; }

        // For SVGTests
        virtual bool isValid() const { return true; }

        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual bool childShouldCreateRenderer(Node*) const;

        virtual void svgAttributeChanged(const QualifiedName&) { }
        virtual void synchronizeProperty(const QualifiedName&) { }

        void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
        
        virtual AffineTransform* supplementalTransform() { return 0; }

        void invalidateSVGAttributes() { clearAreSVGAttributesValid(); }

        const HashSet<SVGElementInstance*>& instancesForElement() const;

        void setCursorElement(SVGCursorElement*);
        void cursorElementRemoved();
        void setCursorImageValue(CSSCursorImageValue*);
        void cursorImageValueRemoved();

        virtual void updateAnimatedSVGAttribute(const QualifiedName&) const;

    protected:
        SVGElement(const QualifiedName&, Document*);

        virtual void finishParsingChildren();
        virtual void insertedIntoDocument();
        virtual void attributeChanged(Attribute*, bool preserveDecls = false);

        SVGElementRareData* rareSVGData() const;
        SVGElementRareData* ensureRareSVGData();

    private:
        friend class SVGElementInstance;

        virtual bool isSupported(StringImpl* feature, StringImpl* version) const;

        virtual ContainerNode* eventParentNode();
        virtual void buildPendingResource() { }

        void mapInstanceToElement(SVGElementInstance*);
        void removeInstanceMapping(SVGElementInstance*);

        virtual bool haveLoadedRequiredResources();
    };

}

// This file needs to be included after the SVGElement declaration
#include "SVGAnimatedProperty.h"

#endif
#endif
