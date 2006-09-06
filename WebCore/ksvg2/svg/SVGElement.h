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
#ifdef SVG_SUPPORT

#include "StyledElement.h"
#include "SVGNames.h"
#include "SVGDocumentExtensions.h"
#include "Document.h"

// FIXME: Templatify as much as possible here!
#define ANIMATED_PROPERTY_DECLARATIONS(BareType, StorageType, UpperProperty, LowerProperty) \
public: \
    BareType LowerProperty() const; \
    void set##UpperProperty(BareType newValue); \
    BareType LowerProperty##BaseValue() const; \
    void set##UpperProperty##BaseValue(BareType newValue); \
private: \
    StorageType m_##LowerProperty;

#define ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, ContextElement) \
BareType ClassName::LowerProperty() const \
{ \
    return StorageGetter; \
} \
void ClassName::set##UpperProperty(BareType newValue) \
{ \
    m_##LowerProperty = newValue; \
} \
BareType ClassName::LowerProperty##BaseValue() const \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context != 0); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<BareType>(context, AttrName)) \
         return extensions->baseValue<BareType>(context, AttrName); \
    return LowerProperty(); \
} \
void ClassName::set##UpperProperty##BaseValue(BareType newValue) \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context != 0); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<BareType>(context, AttrName)) \
        extensions->setBaseValue<BareType>(context, AttrName, newValue); \
    set##UpperProperty(newValue); \
}

#define ANIMATED_PROPERTY_DEFINITIONS_WITH_CONTEXT(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, contextElement())

#define ANIMATED_PROPERTY_DEFINITIONS(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, this)

namespace WebCore {
    class DocumentPtr;
    class Ecma;
    class SVGMatrix;
    class SVGStyledElement;
    class SVGSVGElement;

    class SVGElement : public StyledElement {
    public:
        SVGElement(const QualifiedName&, Document*);
        virtual ~SVGElement();
        virtual bool isSVGElement() const { return true; }
        virtual bool isSupported(StringImpl* feature, StringImpl* version) const;
        
        String id() const;
        void setId(const String&);
        String xmlbase() const;
        void setXmlbase(const String&);

        SVGSVGElement* ownerSVGElement() const;
        SVGElement* viewportElement() const;

        // Helper methods that returns the attr value if attr is set, otherwise the default value.
        // It throws NO_MODIFICATION_ALLOWED_ERR if the element is read-only.
        AtomicString tryGetAttribute(const String& name, AtomicString defaultValue = AtomicString()) const;
        AtomicString tryGetAttributeNS(const String& namespaceURI, const String& localName, AtomicString defaultValue = AtomicString()) const;

        // Internal
        virtual void parseMappedAttribute(MappedAttribute*);
        
        virtual bool isStyled() const { return false; }
        virtual bool isStyledTransformable() const { return false; }
        virtual bool isStyledLocatable() const { return false; }
        virtual bool isSVG() const { return false; }
        virtual bool isFilterEffect() const { return false; }
        virtual bool isGradientStop() const { return false; }
        
        // For SVGTests
        virtual bool isValid() const { return true; }
        
        virtual void closeRenderer();
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual bool childShouldCreateRenderer(Node*) const;
        
        void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);

    private:
        void addSVGEventListener(const AtomicString& eventType, const Attribute*);
        virtual bool haveLoadedRequiredResources();
    };


    static inline SVGElement* svg_dynamic_cast(Node* node)
    {
        SVGElement* svgElement = NULL;
        if (node && node->isSVGElement())
            svgElement = static_cast<SVGElement*>(node);
        return svgElement;
    }

} // namespace WebCore 

#endif // SVG_SUPPORT
#endif // KSVG_SVGElementImpl_H

// vim:ts=4:noet
