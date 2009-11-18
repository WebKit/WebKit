/*
    Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) Research In Motion Limited 2009. All rights reserved.

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

#ifndef SVGAnimatedProperty_h
#define SVGAnimatedProperty_h

#if ENABLE(SVG)
#include "SVGAnimatedTemplate.h"
#include "SVGDocumentExtensions.h"
#include "SynchronizableTypeWrapper.h"

namespace WebCore {

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    class SVGAnimatedProperty;

    template<typename OwnerType, typename OwnerElement, typename AnimatedType, typename DecoratedType, const char* TagName, const char* PropertyName>
    class SVGAnimatedPropertyTearOff : public SVGAnimatedTemplate<DecoratedType> {
    public:
        typedef SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName> Self;
        typedef SVGAnimatedProperty<OwnerType, AnimatedType, TagName, PropertyName> Creator;

        static PassRefPtr<Self> create(const Creator& creator, const OwnerElement* owner, const QualifiedName& attributeName)
        {
            return adoptRef(new Self(creator, owner, attributeName));
        }

        virtual DecoratedType baseVal() const;
        virtual void setBaseVal(DecoratedType);

        virtual DecoratedType animVal() const;
        virtual void setAnimVal(DecoratedType);

    private:
        SVGAnimatedPropertyTearOff(const Creator&, const OwnerElement*, const QualifiedName& attributeName);

        Creator& m_creator;
        RefPtr<OwnerElement> m_ownerElement;
    };

    // Helper templates mapping owner types to owner elements (for SVG*Element OwnerType is equal to OwnerElement, for non-SVG*Element derived types, they're different)
    template<typename OwnerType, bool isDerivedFromSVGElement>
    struct GetOwnerElementForType;

    template<typename OwnerType>
    struct IsDerivedFromSVGElement;

    // Helper template used for synchronizing SVG <-> XML properties
    template<typename OwnerType, typename DecoratedType, bool isDerivedFromSVGElement>
    struct PropertySynchronizer;

    // Abstract base class
    class SVGAnimatedPropertyBase : public Noncopyable {
    public:
        virtual ~SVGAnimatedPropertyBase() { }
        virtual void synchronize() const = 0;
        virtual void startAnimation() const = 0;
        virtual void stopAnimation() = 0;
    };

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    class SVGAnimatedProperty : public SVGAnimatedPropertyBase {
    public:
        typedef OwnerTypeArg OwnerType;
        typedef AnimatedTypeArg AnimatedType;

        typedef typename SVGAnimatedTypeValue<AnimatedType>::StorableType StorableType;
        typedef typename SVGAnimatedTypeValue<AnimatedType>::DecoratedType DecoratedType;

        typedef GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value> OwnerElementForType;
        typedef typename OwnerElementForType::OwnerElement OwnerElement;
        typedef SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName> TearOff;

        // attributeName & attributeIdentifier may differ. For SVGMarkerElement, there are two exposed SVG animatable
        // properties: orientType & orientAngle, though only one DOM attribute "orient", handle these cases!
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName);
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName, const AtomicString& attributeIdentifier);

        // "Forwarding constructors" for primitive type assignment with more than one argument
        template<typename T1>
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName,
                            const T1&);

        template<typename T1>
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName, const AtomicString& attributeIdentifier,
                            const T1&);

        template<typename T1, typename T2>
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName,
                            const T1&, const T2&);

        template<typename T1, typename T2>
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName, const AtomicString& attributeIdentifier,
                            const T1&, const T2&);

        template<typename T1, typename T2, typename T3>
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName,
                            const T1&, const T2&, const T3&);
    
        template<typename T1, typename T2, typename T3>
        SVGAnimatedProperty(const OwnerType*, const QualifiedName& attributeName, const AtomicString& attributeIdentifier,
                            const T1&, const T2&, const T3&);

        DecoratedType value() const;
        void setValue(DecoratedType);

        DecoratedType baseValue() const;
        void setBaseValue(DecoratedType);

        // Tear offs only used by bindings, never in internal code
        PassRefPtr<TearOff> animatedTearOff() const;

        void registerProperty();
        virtual void synchronize() const;

        void startAnimation() const;
        void stopAnimation();

    private:
        const OwnerElement* ownerElement() const;

    private:
        // We're a member variable on stack, living in OwnerType, NO need to ref here.
        const OwnerType* m_ownerType;

        const QualifiedName& m_attributeName;
        const AtomicString& m_attributeIdentifier;

        mutable SynchronizableTypeWrapper<StorableType> m_value;

#ifndef NDEBUG
        bool m_registered;
#endif
    };

    // SVGAnimatedPropertyTearOff implementation
    template<typename OwnerType, typename OwnerElement, typename AnimatedType, typename DecoratedType, const char* TagName, const char* PropertyName>
    SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName>::SVGAnimatedPropertyTearOff(const Creator& creator,
                                                                                                                                        const OwnerElement* owner,
                                                                                                                                        const QualifiedName& attributeName)
        : SVGAnimatedTemplate<DecoratedType>(attributeName)
        , m_creator(const_cast<Creator&>(creator))
        , m_ownerElement(const_cast<OwnerElement*>(owner))
    {
        ASSERT(m_ownerElement);
    }

    template<typename OwnerType, typename OwnerElement, typename AnimatedType, typename DecoratedType, const char* TagName, const char* PropertyName>
    DecoratedType SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName>::baseVal() const
    {
        return m_creator.baseValue();
    }

    template<typename OwnerType, typename OwnerElement, typename AnimatedType, typename DecoratedType, const char* TagName, const char* PropertyName>
    void SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName>::setBaseVal(DecoratedType newBaseVal)
    {
        m_creator.setBaseValue(newBaseVal);
    }

    template<typename OwnerType, typename OwnerElement, typename AnimatedType, typename DecoratedType, const char* TagName, const char* PropertyName>
    DecoratedType SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName>::animVal() const
    {
        return m_creator.value();
    }

    template<typename OwnerType, typename OwnerElement, typename AnimatedType, typename DecoratedType, const char* TagName, const char* PropertyName>
    void SVGAnimatedPropertyTearOff<OwnerType, OwnerElement, AnimatedType, DecoratedType, TagName, PropertyName>::setAnimVal(DecoratedType newAnimVal)
    {
        m_creator.setValue(newAnimVal);
    }

    // SVGAnimatedProperty implementation
    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeName.localName())
        , m_value()
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const AtomicString& attributeIdentifier)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeIdentifier)
        , m_value()
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    template<typename T1>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const T1& arg1)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeName.localName())
        , m_value(arg1)
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    template<typename T1>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const AtomicString& attributeIdentifier,
                                                                                                   const T1& arg1)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeIdentifier)
        , m_value(arg1)
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    template<typename T1, typename T2>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const T1& arg1,
                                                                                                   const T2& arg2)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeName.localName())
        , m_value(arg1, arg2)
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    template<typename T1, typename T2>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const AtomicString& attributeIdentifier,
                                                                                                   const T1& arg1,
                                                                                                   const T2& arg2)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeIdentifier)
        , m_value(arg1, arg2)
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    template<typename T1, typename T2, typename T3>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const T1& arg1,
                                                                                                   const T2& arg2,
                                                                                                   const T3& arg3)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeName.localName())
        , m_value(arg1, arg2, arg3)
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    template<typename T1, typename T2, typename T3>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::SVGAnimatedProperty(const OwnerType* owner,
                                                                                                   const QualifiedName& attributeName,
                                                                                                   const AtomicString& attributeIdentifier,
                                                                                                   const T1& arg1,
                                                                                                   const T2& arg2,
                                                                                                   const T3& arg3)
        : m_ownerType(owner)
        , m_attributeName(attributeName)
        , m_attributeIdentifier(attributeIdentifier)
        , m_value(arg1, arg2, arg3)
#ifndef NDEBUG
        , m_registered(false)
#endif
    {
        ASSERT(m_ownerType);
        registerProperty();
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    typename SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::DecoratedType
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::value() const
    {
        ASSERT(m_registered);
        return m_value;
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    void SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::setValue(typename SVGAnimatedProperty::DecoratedType newValue)
    {
        ASSERT(m_registered);
        m_value = newValue;
        ownerElement()->setSynchronizedSVGAttributes(false);
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    typename SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::DecoratedType
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::baseValue() const
    {
        ASSERT(m_registered);
        const OwnerElement* ownerElement = this->ownerElement();
        SVGDocumentExtensions* extensions = ownerElement->accessDocumentSVGExtensions();
        if (extensions && extensions->hasBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier))
            return extensions->baseValue<DecoratedType>(ownerElement, m_attributeIdentifier);

        return m_value;
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    void SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::setBaseValue(typename SVGAnimatedProperty::DecoratedType newValue)
    {
        ASSERT(m_registered);
        const OwnerElement* ownerElement = this->ownerElement();
        SVGDocumentExtensions* extensions = ownerElement->accessDocumentSVGExtensions();
        if (extensions && extensions->hasBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier)) {
            extensions->setBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier, newValue);
            return;
        }

        // Only update stored property, if not animating
        m_value = newValue;
        ownerElement->setSynchronizedSVGAttributes(false);
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    PassRefPtr<typename SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::TearOff>
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::animatedTearOff() const
    {
        ASSERT(m_registered);
        return lookupOrCreateWrapper<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName, TearOff, OwnerElement>(*this, ownerElement(), m_attributeName, m_attributeIdentifier);
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    void SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::registerProperty()
    {
        ASSERT(!m_registered);
        ownerElement()->propertyController().registerProperty(m_attributeName, this);

#ifndef NDEBUG
        m_registered = true;
#endif
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    void SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::synchronize() const
    {
        ASSERT(m_registered);
        if (!m_value.needsSynchronization()) 
            return; 

        PropertySynchronizer<OwnerElement, DecoratedType, IsDerivedFromSVGElement<OwnerType>::value>::synchronize(ownerElement(), m_attributeName, baseValue());
        m_value.setSynchronized(); 
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    void SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::startAnimation() const
    {
        ASSERT(m_registered);
        const OwnerElement* ownerElement = this->ownerElement();
        if (SVGDocumentExtensions* extensions = ownerElement->accessDocumentSVGExtensions()) {
            ASSERT(!extensions->hasBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier));
            extensions->setBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier, m_value);
        }
    }
    
    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    void SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::stopAnimation()
    {
        ASSERT(m_registered);
        const OwnerElement* ownerElement = this->ownerElement();
        if (SVGDocumentExtensions* extensions = ownerElement->accessDocumentSVGExtensions()) {
            ASSERT(extensions->hasBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier));
            setValue(extensions->baseValue<DecoratedType>(ownerElement, m_attributeIdentifier));
            extensions->removeBaseValue<DecoratedType>(ownerElement, m_attributeIdentifier);
        }
    }

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    const typename SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::OwnerElement*
    SVGAnimatedProperty<OwnerTypeArg, AnimatedTypeArg, TagName, PropertyName>::ownerElement() const
    {
        return OwnerElementForType::ownerElement(m_ownerType);
    }

    // GetOwnerElementForType implementation
    template<typename OwnerType>
    struct GetOwnerElementForType<OwnerType, true> : Noncopyable {
        typedef OwnerType OwnerElement;

        static const OwnerElement* ownerElement(const OwnerType* type)
        {
            return type;
        }
    };

    template<typename OwnerType>
    struct GetOwnerElementForType<OwnerType, false> : Noncopyable {    
        typedef SVGElement OwnerElement;

        static const OwnerElement* ownerElement(const OwnerType* type)
        {
            const OwnerElement* context = type->contextElement();
            ASSERT(context);
            return context;
        }
    };

    // IsDerivedFromSVGElement implementation
    template<typename OwnerType>
    struct IsDerivedFromSVGElement : Noncopyable {
        static const bool value = true;
    };

    class SVGViewSpec;
    template<>
    struct IsDerivedFromSVGElement<SVGViewSpec> : Noncopyable {
        static const bool value = false;
    };

    // PropertySynchronizer implementation
    template<typename OwnerElement, typename DecoratedType>
    struct PropertySynchronizer<OwnerElement, DecoratedType, true> : Noncopyable {
        static void synchronize(const OwnerElement* ownerElement, const QualifiedName& attributeName, DecoratedType baseValue)
        {
            AtomicString value(SVGAnimatedTypeValue<DecoratedType>::toString(baseValue));

            NamedNodeMap* namedAttrMap = ownerElement->attributes(false); 
            Attribute* old = namedAttrMap->getAttributeItem(attributeName);
            if (old && value.isNull()) 
                namedAttrMap->removeAttribute(old->name()); 
            else if (!old && !value.isNull()) 
                namedAttrMap->addAttribute(const_cast<OwnerElement*>(ownerElement)->createAttribute(attributeName, value));
            else if (old && !value.isNull()) 
                old->setValue(value); 
        }
    };

    template<typename OwnerElement, typename DecoratedType>
    struct PropertySynchronizer<OwnerElement, DecoratedType, false> : Noncopyable {
        static void synchronize(const OwnerElement*, const QualifiedName&, DecoratedType)
        {
            // no-op, for types not inheriting from Element, thus nothing to synchronize
        }
    };

    // Helper macro used to register animated properties within SVG* classes
    #define ANIMATED_PROPERTY_DECLARATIONS(OwnerType, ElementTag, AttributeTag, AnimatedType, UpperProperty, LowerProperty) \
    private: \
        typedef SVGAnimatedProperty<OwnerType, AnimatedType, ElementTag, AttributeTag> SVGAnimatedProperty##UpperProperty; \
        typedef SVGAnimatedTypeValue<AnimatedType>::DecoratedType DecoratedTypeFor##UpperProperty; \
        SVGAnimatedProperty##UpperProperty m_##LowerProperty; \
    public: \
        DecoratedTypeFor##UpperProperty LowerProperty() const { return m_##LowerProperty.value(); } \
        void set##UpperProperty(DecoratedTypeFor##UpperProperty type) { m_##LowerProperty.setValue(type); } \
        DecoratedTypeFor##UpperProperty LowerProperty##BaseValue() const { return m_##LowerProperty.baseValue(); } \
        void set##UpperProperty##BaseValue(DecoratedTypeFor##UpperProperty type) { m_##LowerProperty.setBaseValue(type); } \
        PassRefPtr<SVGAnimatedProperty##UpperProperty::TearOff> LowerProperty##Animated() const { return m_##LowerProperty.animatedTearOff(); }

};

#endif
#endif
