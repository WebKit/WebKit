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
#include "SynchronizableTypeWrapper.h"

#define ANIMATED_PROPERTY_EMPTY_DECLARATIONS_INTERNAL(DecoratedType, NullType, UpperProperty, LowerProperty) \
public: \
    virtual DecoratedType LowerProperty() const { ASSERT_NOT_REACHED(); return NullType; } \
    virtual void set##UpperProperty(DecoratedType) { ASSERT_NOT_REACHED(); } \
    virtual DecoratedType LowerProperty##BaseValue() const { ASSERT_NOT_REACHED(); return NullType; } \
    virtual void set##UpperProperty##BaseValue(DecoratedType) { ASSERT_NOT_REACHED(); } \
    virtual void synchronize##UpperProperty() const { ASSERT_NOT_REACHED(); } \
    virtual AnimatedPropertySynchronizer synchronizerFor##UpperProperty() const { ASSERT_NOT_REACHED(); return AnimatedPropertySynchronizer(); } \
    virtual void start##UpperProperty() const { ASSERT_NOT_REACHED(); } \
    virtual void stop##UpperProperty() { ASSERT_NOT_REACHED(); }

#define ANIMATED_PROPERTY_FORWARD_DECLARATIONS_INTERNAL(ForwardClass, DecoratedType, UpperProperty, LowerProperty) \
public: \
    virtual DecoratedType LowerProperty() const { return ForwardClass::LowerProperty(); } \
    virtual void set##UpperProperty(DecoratedType newValue) { ForwardClass::set##UpperProperty(newValue); } \
    virtual DecoratedType LowerProperty##BaseValue() const { return ForwardClass::LowerProperty##BaseValue(); } \
    virtual void set##UpperProperty##BaseValue(DecoratedType newValue) { ForwardClass::set##UpperProperty##BaseValue(newValue); } \
    virtual void synchronize##UpperProperty() const { ForwardClass::synchronize##UpperProperty(); } \
    virtual AnimatedPropertySynchronizer synchronizerFor##UpperProperty() const { return ForwardClass::synchronizerFor##UpperProperty(); } \
    virtual void start##UpperProperty() const { ForwardClass::start##UpperProperty(); } \
    virtual void stop##UpperProperty() { ForwardClass::stop##UpperProperty(); }

#define ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassType, DecoratedType, StorageType, UpperProperty, LowerProperty) \
class SVGAnimatedTemplate##UpperProperty : public SVGAnimatedTemplate<DecoratedType> { \
public: \
    static PassRefPtr<SVGAnimatedTemplate##UpperProperty> create(const ClassType* element, const QualifiedName& attributeName) \
    { \
        return adoptRef(new SVGAnimatedTemplate##UpperProperty(element, attributeName)); \
    } \
    virtual DecoratedType baseVal() const; \
    virtual void setBaseVal(DecoratedType); \
    virtual DecoratedType animVal() const; \
    virtual void setAnimVal(DecoratedType); \
    \
private: \
    SVGAnimatedTemplate##UpperProperty(const ClassType*, const QualifiedName&); \
    RefPtr<ClassType> m_element; \
}; \
public: \
    DecoratedType LowerProperty() const; \
    void set##UpperProperty(DecoratedType); \
    DecoratedType LowerProperty##BaseValue() const; \
    void set##UpperProperty##BaseValue(DecoratedType); \
    PassRefPtr<SVGAnimatedTemplate##UpperProperty> LowerProperty##Animated() const; \
    void synchronize##UpperProperty() const; \
    AnimatedPropertySynchronizer synchronizerFor##UpperProperty() const; \
    void start##UpperProperty() const; \
    void stop##UpperProperty(); \
\
private: \
    mutable SynchronizableTypeWrapper<StorageType> m_##LowerProperty;

#define ANIMATED_PROPERTY_START_DECLARATIONS \
public: \
    virtual void invokeSVGPropertySynchronizer(StringImpl* stringImpl) const \
    { \
        if (m_svgPropertyUpdateMap.contains(stringImpl)) { \
            AnimatedPropertySynchronizer updateMethod = m_svgPropertyUpdateMap.get(stringImpl); \
            (*updateMethod)(this); \
        } \
    } \
\
    virtual void invokeAllSVGPropertySynchronizers() const \
    { \
        HashMap<StringImpl*, AnimatedPropertySynchronizer>::const_iterator it = m_svgPropertyUpdateMap.begin(); \
        const HashMap<StringImpl*, AnimatedPropertySynchronizer>::const_iterator end = m_svgPropertyUpdateMap.end(); \
        for (; it != end; ++it) { \
            AnimatedPropertySynchronizer updateMethod = it->second; \
            (*updateMethod)(this); \
        } \
    } \
\
    virtual void addSVGPropertySynchronizer(const QualifiedName& attrName, AnimatedPropertySynchronizer method) const \
    { \
        m_svgPropertyUpdateMap.set(attrName.localName().impl(), method); \
    } \
\
private: \
    mutable HashMap<StringImpl*, AnimatedPropertySynchronizer> m_svgPropertyUpdateMap;

#define ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassType, DecoratedType, UpperProperty, LowerProperty, AttrName, AttrIdentifier, ContextElement) \
void synchronize##UpperProperty##ClassName##CallBack(const SVGElement* element) \
{ \
    static_cast<const ClassType*>(element)->synchronize##UpperProperty(); \
} \
ClassName::SVGAnimatedTemplate##UpperProperty::SVGAnimatedTemplate##UpperProperty(const ClassType* element, const QualifiedName& attributeName) \
: SVGAnimatedTemplate<DecoratedType>(attributeName), m_element(const_cast<ClassType*>(element)) { } \
\
DecoratedType ClassName::SVGAnimatedTemplate##UpperProperty::baseVal() const \
{ \
    return m_element->LowerProperty##BaseValue(); \
} \
void ClassName::SVGAnimatedTemplate##UpperProperty::setBaseVal(DecoratedType newBaseVal) \
{ \
    m_element->set##UpperProperty##BaseValue(newBaseVal); \
} \
DecoratedType ClassName::SVGAnimatedTemplate##UpperProperty::animVal() const \
{ \
    return m_element->LowerProperty(); \
} \
void ClassName::SVGAnimatedTemplate##UpperProperty::setAnimVal(DecoratedType newAnimVal) \
{ \
    m_element->set##UpperProperty(newAnimVal); \
} \
DecoratedType ClassName::LowerProperty() const \
{ \
    return m_##LowerProperty; \
} \
void ClassName::set##UpperProperty(DecoratedType newValue) \
{ \
    m_##LowerProperty = newValue; \
    \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    context->setSynchronizedSVGAttributes(false); \
} \
DecoratedType ClassName::LowerProperty##BaseValue() const \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<DecoratedType>(context, AttrIdentifier)) \
         return extensions->baseValue<DecoratedType>(context, AttrIdentifier); \
    return LowerProperty(); \
} \
\
void ClassName::set##UpperProperty##BaseValue(DecoratedType newValue) \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<DecoratedType>(context, AttrIdentifier)) { \
        extensions->setBaseValue<DecoratedType>(context, AttrIdentifier, newValue); \
        return; \
    } \
    /* Only update stored property, if not animating */ \
    set##UpperProperty(newValue); \
} \
\
void ClassName::synchronize##UpperProperty() const \
{ \
    if (!m_##LowerProperty.needsSynchronization()) \
        return; \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    RefPtr<ClassName::SVGAnimatedTemplate##UpperProperty> animatedClass(LowerProperty##Animated()); \
    ASSERT(animatedClass); \
    AtomicString value(SVGAnimatedTypeValue<DecoratedType>::toString(animatedClass->baseVal())); \
    NamedAttrMap* namedAttrMap = context->attributes(false); \
    Attribute* old = namedAttrMap->getAttributeItem(AttrName); \
    if (old && value.isNull()) \
        namedAttrMap->removeAttribute(old->name()); \
    else if (!old && !value.isNull()) \
        namedAttrMap->addAttribute(const_cast<SVGElement*>(context)->createAttribute(QualifiedName(nullAtom, AttrIdentifier, nullAtom), value)); \
    else if (old && !value.isNull()) \
        old->setValue(value); \
    m_##LowerProperty.setSynchronized(); \
} \
\
void ClassName::start##UpperProperty() const \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions) { \
        ASSERT(!extensions->hasBaseValue<DecoratedType>(context, AttrIdentifier)); \
        extensions->setBaseValue<DecoratedType>(context, AttrIdentifier, LowerProperty()); \
    } \
} \
\
void ClassName::stop##UpperProperty() \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions) { \
        ASSERT(extensions->hasBaseValue<DecoratedType>(context, AttrIdentifier)); \
        set##UpperProperty(extensions->baseValue<DecoratedType>(context, AttrIdentifier)); \
        extensions->removeBaseValue<DecoratedType>(context, AttrIdentifier); \
    } \
} \
\
AnimatedPropertySynchronizer ClassName::synchronizerFor##UpperProperty() const \
{ \
    return &synchronize##UpperProperty##ClassName##CallBack; \
} \
PassRefPtr<ClassName::SVGAnimatedTemplate##UpperProperty> ClassName::LowerProperty##Animated() const \
{ \
    const ClassType* context = ContextElement; \
    ASSERT(context); \
    return lookupOrCreateWrapper<ClassName::SVGAnimatedTemplate##UpperProperty, ClassType>(context, AttrName, AttrIdentifier, synchronizerFor##UpperProperty()); \
}

// Forward declarations, used in classes that inherit from SVGElement, and another base class ie. SVGURIReference - which declared it's properties using the WITH_CONTEXT(...) macros)
#define ANIMATED_PROPERTY_FORWARD_DECLARATIONS(ForwardClass, BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_FORWARD_DECLARATIONS_INTERNAL(ForwardClass, BareType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_FORWARD_DECLARATIONS_REFCOUNTED(ForwardClass, BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_FORWARD_DECLARATIONS_INTERNAL(ForwardClass, BareType*, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_EMPTY_DECLARATIONS(BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_EMPTY_DECLARATIONS_INTERNAL(BareType, BareType(), UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_EMPTY_DECLARATIONS_REFCOUNTED(BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_EMPTY_DECLARATIONS_INTERNAL(BareType*, 0, UpperProperty, LowerProperty)

// Macros used for primitive types in SVGElement derived classes
#define ANIMATED_PROPERTY_DECLARATIONS(ClassName, BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassName, BareType, BareType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DEFINITIONS(ClassName, BareType, UpperProperty, LowerProperty, AttrName) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassName, BareType, UpperProperty, LowerProperty, AttrName, AttrName.localName(), this)

#define ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(ClassName, BareType, UpperProperty, LowerProperty, AttrName, AttrIdentifier) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassName, BareType, UpperProperty, LowerProperty, AttrName, AttrIdentifier, this)

// Macros used for refcounted types in SVGElement derived classes
#define ANIMATED_PROPERTY_DECLARATIONS_REFCOUNTED(ClassName, BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassName, BareType*, RefPtr<BareType>, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DEFINITIONS_REFCOUNTED(ClassName, BareType, UpperProperty, LowerProperty, AttrName) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassName, BareType*, UpperProperty, LowerProperty, AttrName, AttrName.localName(), this)

#define ANIMATED_PROPERTY_DEFINITIONS_REFCOUNTED_WITH_CUSTOM_IDENTIFIER(ClassName, BareType, UpperProperty, LowerProperty, AttrName, AttrIdentifier) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassName, BareType*, UpperProperty, LowerProperty, AttrName, AttrIdentifier, this)

// Macros used for primitive types in non-SVGElement base classes (ie. SVGURIReference acts as base class, glued using multiple inheritance in the SVG*Element classes)
#define ANIMATED_PROPERTY_DECLARATIONS_WITH_CONTEXT(ClassName, BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(SVGElement, BareType, BareType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DEFINITIONS_WITH_CONTEXT(ClassName, BareType, UpperProperty, LowerProperty, AttrName) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, SVGElement, BareType, UpperProperty, LowerProperty, AttrName, AttrName.localName(), contextElement())

// Macros used for refcounted types in non-SVGElement base classes (ie. see above)
#define ANIMATED_PROPERTY_DECLARATIONS_REFCOUNTED_WITH_CONTEXT(ClassName, BareType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(SVGElement, BareType*, RefPtr<BareType>, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DEFINITIONS_REFCOUNTED_WITH_CONTEXT(ClassName, BareType, UpperProperty, LowerProperty, AttrName) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, SVGElement, BareType*, UpperProperty, LowerProperty, AttrName, AttrName.localName(), contextElement())

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
        
        virtual AffineTransform* supplementalTransform() { return 0; }

        // Forwarded properties (declared/defined anywhere else in the inheritance structure)

        // -> For SVGURIReference
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(String, Href, href)

        // -> For SVGFitToViewBox
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(FloatRect, ViewBox, viewBox)    
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS_REFCOUNTED(SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio)

        // -> For SVGExternalResourcesRequired
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(bool, ExternalResourcesRequired, externalResourcesRequired)

        virtual bool dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent = false);

        virtual void invokeSVGPropertySynchronizer(StringImpl*) const { }
        virtual void invokeAllSVGPropertySynchronizers() const { }
        virtual void addSVGPropertySynchronizer(const QualifiedName&, AnimatedPropertySynchronizer) const { ASSERT_NOT_REACHED(); }

        virtual void updateAnimatedSVGAttribute(StringImpl*) const;
        virtual void setSynchronizedSVGAttributes(bool) const;

    private:
        void addSVGEventListener(const AtomicString& eventType, const Attribute*);
        virtual bool haveLoadedRequiredResources();

        Node* m_shadowParent;
    };

} // namespace WebCore 

#endif // ENABLE(SVG)
#endif // SVGElement_h
