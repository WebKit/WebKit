/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGElement_h
#define SVGElement_h

#if ENABLE(SVG)
#include "Document.h"
#include "FloatRect.h"
#include "StyledElement.h"
#include "SVGAnimatedTemplate.h"
#include "SVGDocumentExtensions.h"
#include "SVGNames.h"

#define ANIMATED_PROPERTY_EMPTY_DECLARATIONS(BareType, NullType, UpperProperty, LowerProperty) \
public: \
    virtual BareType LowerProperty() const { ASSERT_NOT_REACHED(); return NullType; } \
    virtual void set##UpperProperty(BareType newValue) { ASSERT_NOT_REACHED(); }\
    virtual BareType LowerProperty##BaseValue() const { ASSERT_NOT_REACHED(); return NullType; } \
    virtual void set##UpperProperty##BaseValue(BareType newValue) { ASSERT_NOT_REACHED(); } \
    virtual void start##UpperProperty() const { ASSERT_NOT_REACHED(); } \
    virtual void stop##UpperProperty() { ASSERT_NOT_REACHED(); }

#define ANIMATED_PROPERTY_FORWARD_DECLARATIONS(ForwardClass, BareType, UpperProperty, LowerProperty) \
public: \
    virtual BareType LowerProperty() const { return ForwardClass::LowerProperty(); } \
    virtual void set##UpperProperty(BareType newValue) { ForwardClass::set##UpperProperty(newValue); } \
    virtual BareType LowerProperty##BaseValue() const { return ForwardClass::LowerProperty##BaseValue(); } \
    virtual void set##UpperProperty##BaseValue(BareType newValue) { ForwardClass::set##UpperProperty##BaseValue(newValue); } \
    virtual void start##UpperProperty() const { ForwardClass::start##UpperProperty(); } \
    virtual void stop##UpperProperty() { ForwardClass::stop##UpperProperty(); }

#define ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassType, ClassStorageType, BareType, StorageType, UpperProperty, LowerProperty) \
class SVGAnimatedTemplate##UpperProperty \
: public SVGAnimatedTemplate<BareType> \
{ \
public: \
    SVGAnimatedTemplate##UpperProperty(const ClassType*, const QualifiedName&); \
    virtual ~SVGAnimatedTemplate##UpperProperty() { } \
    virtual BareType baseVal() const; \
    virtual void setBaseVal(BareType); \
    virtual BareType animVal() const; \
    virtual void setAnimVal(BareType); \
    \
protected: \
    ClassStorageType m_element; \
}; \
public: \
    BareType LowerProperty() const; \
    void set##UpperProperty(BareType); \
    BareType LowerProperty##BaseValue() const; \
    void set##UpperProperty##BaseValue(BareType); \
    PassRefPtr<SVGAnimatedTemplate##UpperProperty> LowerProperty##Animated() const; \
    void start##UpperProperty() const; \
    void stop##UpperProperty(); \
\
private: \
    StorageType m_##LowerProperty;

#define ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassType, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, ContextElement) \
ClassName::SVGAnimatedTemplate##UpperProperty::SVGAnimatedTemplate##UpperProperty(const ClassType* element, const QualifiedName& attributeName) \
: SVGAnimatedTemplate<BareType>(attributeName), m_element(const_cast<ClassType*>(element)) { } \
\
BareType ClassName::SVGAnimatedTemplate##UpperProperty::baseVal() const \
{ \
    return m_element->LowerProperty##BaseValue(); \
} \
void ClassName::SVGAnimatedTemplate##UpperProperty::setBaseVal(BareType newBaseVal) \
{ \
    m_element->set##UpperProperty##BaseValue(newBaseVal); \
} \
BareType ClassName::SVGAnimatedTemplate##UpperProperty::animVal() const \
{ \
    return m_element->LowerProperty(); \
} \
void ClassName::SVGAnimatedTemplate##UpperProperty::setAnimVal(BareType newAnimVal) \
{ \
    m_element->set##UpperProperty(newAnimVal); \
} \
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
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<BareType>(context, AttrName)) \
         return extensions->baseValue<BareType>(context, AttrName); \
    return LowerProperty(); \
} \
void ClassName::set##UpperProperty##BaseValue(BareType newValue) \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<BareType>(context, AttrName)) { \
        extensions->setBaseValue<BareType>(context, AttrName, newValue); \
        return; \
    } \
    /* Only update stored property, if not animating */ \
    set##UpperProperty(newValue); \
} \
\
void ClassName::start##UpperProperty() const \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions) { \
        ASSERT(!extensions->hasBaseValue<BareType>(context, AttrName)); \
        extensions->setBaseValue<BareType>(context, AttrName, LowerProperty()); \
    } \
} \
\
void ClassName::stop##UpperProperty() \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions) { \
        ASSERT(extensions->hasBaseValue<BareType>(context, AttrName)); \
        set##UpperProperty(extensions->baseValue<BareType>(context, AttrName)); \
        extensions->removeBaseValue<BareType>(context, AttrName); \
    } \
}

// These are the macros which will be used to declare/implement the svg animated properties...
#define ANIMATED_PROPERTY_DECLARATIONS_WITH_CONTEXT(ClassName, BareType, StorageType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(SVGElement, RefPtr<SVGElement>, BareType, StorageType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DECLARATIONS(ClassName, BareType, StorageType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassName, RefPtr<ClassName>, BareType, StorageType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DEFINITIONS_WITH_CONTEXT(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, SVGElement, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName.localName(), StorageGetter, contextElement()) \
PassRefPtr<ClassName::SVGAnimatedTemplate##UpperProperty> ClassName::LowerProperty##Animated() const \
{ \
    const SVGElement* context = contextElement(); \
    ASSERT(context); \
    return lookupOrCreateWrapper<ClassName::SVGAnimatedTemplate##UpperProperty, SVGElement>(context, AttrName, AttrName.localName()); \
}

#define ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, AttrIdentifier, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName.localName(), StorageGetter, this) \
PassRefPtr<ClassName::SVGAnimatedTemplate##UpperProperty> ClassName::LowerProperty##Animated() const \
{ \
    return lookupOrCreateWrapper<ClassName::SVGAnimatedTemplate##UpperProperty, ClassName>(this, AttrName, AttrIdentifier); \
}

#define ANIMATED_PROPERTY_DEFINITIONS(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, AttrName.localName(), StorageGetter)

namespace WebCore {

    class SVGPreserveAspectRatio;
    class SVGSVGElement;

    class SVGElement : public StyledElement {
    public:
        SVGElement(const QualifiedName&, Document*);
        virtual ~SVGElement();
        virtual bool isSVGElement() const { return true; }
        virtual bool isSupported(StringImpl* feature, StringImpl* version) const;
        
        String id() const;
        void setId(const String&, ExceptionCode&);
        String xmlbase() const;
        void setXmlbase(const String&, ExceptionCode&);

        SVGSVGElement* ownerSVGElement() const;
        SVGElement* viewportElement() const;

        virtual void parseMappedAttribute(MappedAttribute*);

        virtual bool isStyled() const { return false; }
        virtual bool isStyledTransformable() const { return false; }
        virtual bool isStyledLocatable() const { return false; }
        virtual bool isSVG() const { return false; }
        virtual bool isFilterEffect() const { return false; }
        virtual bool isGradientStop() const { return false; }
        virtual bool isTextContent() const { return false; }

        virtual bool isShadowNode() const { return m_shadowParent; }
        virtual Node* shadowParentNode() { return m_shadowParent; }
        void setShadowParentNode(Node* node) { m_shadowParent = node; }
        virtual Node* eventParentNode() { return isShadowNode() ? shadowParentNode() : parentNode(); }

        // For SVGTests
        virtual bool isValid() const { return true; }
  
        virtual void finishParsingChildren();
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual bool childShouldCreateRenderer(Node*) const;

        virtual void insertedIntoDocument();
        virtual void buildPendingResource() { }

        virtual void svgAttributeChanged(const QualifiedName&) { }
        virtual void attributeChanged(Attribute*, bool preserveDecls = false);

        void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);

        // Forwarded properties (declared/defined anywhere else in the inheritance structure)

        // -> For SVGURIReference
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(String, String(), Href, href)

        // -> For SVGFitToViewBox
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(FloatRect, FloatRect(), ViewBox, viewBox)    
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(SVGPreserveAspectRatio*, 0, PreserveAspectRatio, preserveAspectRatio)

        // -> For SVGExternalResourcesRequired
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(bool, false, ExternalResourcesRequired, externalResourcesRequired)

        virtual bool dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent = false);

    private:
        void addSVGEventListener(const AtomicString& eventType, const Attribute*);
        virtual bool haveLoadedRequiredResources();

        Node* m_shadowParent;
    };

} // namespace WebCore 

#endif // ENABLE(SVG)
#endif // SVGElement_h
