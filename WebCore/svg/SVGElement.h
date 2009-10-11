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
#include "StyledElement.h"
#include "SVGAnimatedProperty.h"

namespace WebCore {

    class CSSCursorImageValue;
    class Document;
    class SVGCursorElement;
    class SVGDocumentExtensions;
    class SVGElementInstance;
    class SVGSVGElement;
    class TransformationMatrix;

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

        void setShadowParentNode(ContainerNode* node) { m_shadowParent = node; }

        // For SVGTests
        virtual bool isValid() const { return true; }

        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual bool childShouldCreateRenderer(Node*) const;

        virtual void svgAttributeChanged(const QualifiedName&) { }

        void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
        
        virtual TransformationMatrix* supplementalTransform() { return 0; }

        virtual void setSynchronizedSVGAttributes(bool) const;

        HashSet<SVGElementInstance*> instancesForElement() const;

        void addSVGPropertySynchronizer(const QualifiedName& attrName, const SVGAnimatedPropertyBase& base) const
        {
            m_svgPropertyMap.set(attrName.localName(), &base);
        }

        void setCursorElement(SVGCursorElement* cursorElement) { m_cursorElement = cursorElement; }
        void setCursorImageValue(CSSCursorImageValue* cursorImageValue) { m_cursorImageValue = cursorImageValue; }

    protected:
        SVGElement(const QualifiedName&, Document*);

        virtual void finishParsingChildren();
        virtual void insertedIntoDocument();
        virtual void attributeChanged(Attribute*, bool preserveDecls = false);
        virtual void updateAnimatedSVGAttribute(const String&) const;

    private:
        friend class SVGElementInstance;

        virtual bool isSVGElement() const { return true; }

        virtual bool isSupported(StringImpl* feature, StringImpl* version) const;
        
        virtual bool isShadowNode() const { return m_shadowParent; }
        virtual Node* shadowParentNode() { return m_shadowParent; }
        virtual ContainerNode* eventParentNode();

        virtual void buildPendingResource() { }

        // Inlined methods handling SVG property synchronization
        void invokeSVGPropertySynchronizer(const String& name) const
        {
            if (m_svgPropertyMap.contains(name)) {
                const SVGAnimatedPropertyBase* property = m_svgPropertyMap.get(name);
                ASSERT(property);

                property->synchronize();
            }
        }

        void invokeAllSVGPropertySynchronizers() const
        {
            HashMap<String, const SVGAnimatedPropertyBase*>::const_iterator it = m_svgPropertyMap.begin();
            const HashMap<String, const SVGAnimatedPropertyBase*>::const_iterator end = m_svgPropertyMap.end();
            for (; it != end; ++it) {
                const SVGAnimatedPropertyBase* property = it->second;
                ASSERT(property);

                property->synchronize();
            }
        }

        void mapInstanceToElement(SVGElementInstance*);
        void removeInstanceMapping(SVGElementInstance*);

        virtual bool haveLoadedRequiredResources();

        ContainerNode* m_shadowParent;
        mutable HashMap<String, const SVGAnimatedPropertyBase*> m_svgPropertyMap;

        SVGCursorElement* m_cursorElement;
        CSSCursorImageValue* m_cursorImageValue;

        HashSet<SVGElementInstance*> m_elementInstances;
    };

} // namespace WebCore 

#endif // ENABLE(SVG)
#endif // SVGElement_h
