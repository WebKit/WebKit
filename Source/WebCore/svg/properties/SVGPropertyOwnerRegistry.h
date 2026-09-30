/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGAnimatedPropertyAccessorImpl.h"
#include "SVGAnimatedPropertyPairAccessorImpl.h"
#include "SVGPropertyAccessorImpl.h"
#include "SVGPropertyRegistry.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class SVGRectElement;
class SVGCircleElement;

class SVGAttributeAnimator;
class SVGConditionalProcessingAttributes;

template<typename T>
concept HasFastPropertyForAttribute = requires(const T& element, const QualifiedName& name)
{
    { element.propertyForAttribute(name) } -> std::same_as<SVGAnimatedPropertyBase*>;
};

struct SVGAttributeHashTranslator {
    static unsigned hash(const QualifiedName& key)
    {
        if (key.hasPrefix()) {
            QualifiedNameComponents components = { nullAtom().impl(), key.localName().impl(), key.namespaceURI().impl() };
            return computeHash(components);
        }
        return DefaultHash<QualifiedName>::hash(key);
    }
    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a.matches(b); }

    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    static constexpr bool hasHashInValue = true;
};

template<typename OwnerType, typename... BaseTypes>
class SVGPropertyOwnerRegistryBase {
public:
    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGStringList> SVGConditionalProcessingAttributes::*property>
    static void registerConditionalProcessingAttributeProperty()
    {
        registerProperty(attributeName, SVGConditionalProcessingAttributeAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGTransformList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGTransformListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedBoolean> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedBooleanAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, typename EnumType, const Ref<SVGAnimatedEnumeration> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedEnumerationAccessor<OwnerType, EnumType>::template singleton<property>());
    }

    // initialValue has to agree with what the element passes to SVGAnimatedInteger::create(): that
    // is what the property is born with, and this is only the way back to it.
    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedInteger> OwnerType::*property>
    static void registerProperty(int initialValue = 0)
    {
        registerProperty(attributeName, SVGAnimatedIntegerAccessor<OwnerType>::template singleton<property>(initialValue));
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedLength> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedLengthAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedLengthList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedLengthListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedNumber> OwnerType::*property>
    static void registerProperty(float initialValue = 0)
    {
        registerProperty(attributeName, SVGAnimatedNumberAccessor<OwnerType>::template singleton<property>(initialValue));
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedNumberList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedNumberListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedAngle> OwnerType::*property1, const Ref<SVGAnimatedOrientType> OwnerType::*property2>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedAngleOrientAccessor<OwnerType>::template singleton<property1, property2>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedPath> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedPathAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedPointList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedPointListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedPreserveAspectRatio> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedPreserveAspectRatioAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedRect> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedRectAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedString> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedStringAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedTransformList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedTransformListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedInteger> OwnerType::*property1, const Ref<SVGAnimatedInteger> OwnerType::*property2>
    static void registerProperty(int initialValue1 = 0, int initialValue2 = 0)
    {
        registerProperty(attributeName, SVGAnimatedIntegerPairAccessor<OwnerType>::template singleton<property1, property2>(initialValue1, initialValue2));
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const Ref<SVGAnimatedNumber> OwnerType::*property1, const Ref<SVGAnimatedNumber> OwnerType::*property2>
    static void registerProperty(float initialValue1 = 0, float initialValue2 = 0)
    {
        registerProperty(attributeName, SVGAnimatedNumberPairAccessor<OwnerType>::template singleton<property1, property2>(initialValue1, initialValue2));
    }

    // Enumerate all the SVGMemberAccessors recursively. The functor will be called and will
    // be given the pair<QualifiedName, SVGMemberAccessor> till the functor returns false.
    template<typename Functor>
    static bool enumerateRecursively(NOESCAPE const Functor& functor)
    {
        for (const auto& entry : attributeNameToAccessorMap()) {
            if (!functor(entry))
                return false;
        }
        return enumerateRecursivelyBaseTypes(functor);
    }

    template<typename Functor>
    static bool lookupRecursivelyAndApply(const QualifiedName& attributeName, NOESCAPE const Functor& functor)
    {
        if (auto* accessor = findAccessor(attributeName)) {
            functor(*accessor);
            return true;
        }
        return lookupRecursivelyAndApplyBaseTypes(attributeName, functor);
    }

    // Returns true if OwnerType owns a property whose name is attributeName.
    static bool isKnownAttribute(const QualifiedName& attributeName)
    {
        return findAccessor(attributeName);
    }

    // Returns true if OwnerType owns a property whose name is attributeName
    // and its type is SVGAnimatedLength.
    static bool isAnimatedLengthAttribute(const QualifiedName& attributeName)
    {
        if (const auto* accessor = findAccessor(attributeName))
            return accessor->isAnimatedLength();
        return false;
    }

    static inline SVGAnimatedPropertyBase* fastAnimatedPropertyLookup(const OwnerType& owner, const QualifiedName& attributeName)
    {
        if constexpr (HasFastPropertyForAttribute<OwnerType>)
            return owner.propertyForAttribute(attributeName);
        else {
            static_assert(!std::is_same_v<OwnerType, SVGRectElement> && !std::is_same_v<OwnerType, SVGCircleElement>, "Element should use fast property path");
            return nullptr;
        }
    }

private:
    // Singleton map for every OwnerType.
    using QualifiedNameAccessorHashMap = HashMap<QualifiedName, const SVGMemberAccessor<OwnerType>*, SVGAttributeHashTranslator>;

    static QualifiedNameAccessorHashMap& attributeNameToAccessorMap()
    {
        static NeverDestroyed<QualifiedNameAccessorHashMap> attributeNameToAccessorMap;
        return attributeNameToAccessorMap;
    }

    static void registerProperty(const QualifiedName& attributeName, const SVGMemberAccessor<OwnerType>& propertyAccessor)
    {
        attributeNameToAccessorMap().add(attributeName, &propertyAccessor);
    }

    template<typename Functor, size_t I = 0>
    static bool enumerateRecursivelyBaseTypes(NOESCAPE const Functor&)
        requires (I == sizeof...(BaseTypes))
    {
        return true;
    }

    template<typename Functor, size_t I = 0>
    static bool enumerateRecursivelyBaseTypes(NOESCAPE const Functor& functor)
        requires (I < sizeof...(BaseTypes))
    {
        using BaseType = std::tuple_element_t<I, std::tuple<BaseTypes...>>;

        if (!BaseType::PropertyRegistry::enumerateRecursively(functor))
            return false;

        return enumerateRecursivelyBaseTypes<Functor, I + 1>(functor);
    }

    static const SVGMemberAccessor<OwnerType>* findAccessor(const QualifiedName& attributeName)
    {
        auto it = attributeNameToAccessorMap().find(attributeName);
        return it != attributeNameToAccessorMap().end() ? it->value : nullptr;
    }

    template<typename Functor, size_t I = 0>
    static bool lookupRecursivelyAndApplyBaseTypes(const QualifiedName&, NOESCAPE const Functor&)
        requires (I == sizeof...(BaseTypes))
    {
        return false;
    }

    template<typename Functor, size_t I = 0>
    static bool lookupRecursivelyAndApplyBaseTypes(const QualifiedName& attributeName, NOESCAPE const Functor& functor)
        requires (I < sizeof...(BaseTypes))
    {
        using BaseType = std::tuple_element_t<I, std::tuple<BaseTypes...>>;

        if (BaseType::PropertyRegistry::lookupRecursivelyAndApply(attributeName, functor))
            return true;

        return lookupRecursivelyAndApplyBaseTypes<Functor, I + 1>(attributeName, functor);
    }
};

template<typename OwnerType, typename... BaseTypes>
class SVGPropertyOwnerRegistry : public SVGPropertyRegistry, public SVGPropertyOwnerRegistryBase<OwnerType, BaseTypes...> {
    using Base = SVGPropertyOwnerRegistryBase<OwnerType, BaseTypes...>;
public:
    static const SVGPropertyOwnerRegistry& singleton()
    {
        static NeverDestroyed<SVGPropertyOwnerRegistry> registry;
        return registry;
    }

    using Base::enumerateRecursively;
    using Base::fastAnimatedPropertyLookup;
    using Base::isAnimatedLengthAttribute;
    using Base::lookupRecursivelyAndApply;

    QualifiedName propertyAttributeName(const SVGElement& element, const SVGProperty& property) const override
    {
        QualifiedName attributeName = nullQName();
        enumerateRecursively([&](const auto& entry) -> bool {
            if (!entry.value->matches(owner(element), property))
                return true;
            attributeName = entry.key;
            return false;
        });
        return attributeName;
    }

    QualifiedName animatedPropertyAttributeName(const SVGElement& element, const SVGAnimatedPropertyBase& animatedProperty) const override
    {
        QualifiedName attributeName = nullQName();
        enumerateRecursively([&](const auto& entry) -> bool {
            if (!entry.value->matches(owner(element), animatedProperty))
                return true;
            attributeName = entry.key;
            return false;
        });
        return attributeName;
    }

    void setAnimatedPropertyDirty(const SVGElement& element, const QualifiedName& attributeName, SVGAnimatedPropertyBase& animatedProperty) const override
    {
        if (RefPtr property = fastAnimatedPropertyLookup(owner(element), attributeName)) {
            property->setDirty();
            return;
        }

        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            accessor.setDirty(owner(element), animatedProperty);
        });
    }

    // Found by identity rather than by attribute name, so that the caller does not have to know
    // its own name. matches() is the same pointer comparison used by
    // animatedPropertyAttributeName() above. A pair accessor matches either half and resets both,
    // which is what <number-optional-number> needs.
    void resetAnimatedPropertyBaseVal(const SVGElement& element, const SVGAnimatedPropertyBase& animatedProperty) const override
    {
        bool found = !enumerateRecursively([&](const auto& entry) -> bool {
            if (!entry.value->matches(owner(element), animatedProperty))
                return true;
            entry.value->resetBaseVal(owner(element));
            return false;
        });
        ASSERT_UNUSED(found, found);
    }

    // Detach all the properties recursively from their OwnerTypes.
    void detachAllProperties(const SVGElement& element) const override
    {
        enumerateRecursively([&](const auto& entry) -> bool {
            entry.value->detach(owner(element));
            return true;
        });
    }


    // Finds the property whose name is attributeName and returns the synchronize
    // string through the associated SVGMemberAccessor.
    std::optional<String> synchronize(const SVGElement& element, const QualifiedName& attributeName) const override
    {
        if (RefPtr property = fastAnimatedPropertyLookup(owner(element), attributeName))
            return property->synchronize();

        std::optional<String> value;
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            value = accessor.synchronize(owner(element));
        });
        return value;
    }

    // Enumerate recursively the SVGMemberAccessors of the OwnerType and all its BaseTypes.
    // Collect all the pairs <AttributeName, String> only for the dirty properties.
    HashMap<QualifiedName, String> synchronizeAllAttributes(const SVGElement& element) const override
    {
        HashMap<QualifiedName, String> map;
        enumerateRecursively([&](const auto& entry) -> bool {
            if (auto string = entry.value->synchronize(owner(element)))
                map.add(entry.key, *string);
            return true;
        });
        return map;
    }

    bool isAnimatedPropertyAttribute(const SVGElement& element, const QualifiedName& attributeName) const override
    {
        if (auto* property = fastAnimatedPropertyLookup(owner(element), attributeName))
            return true;

        bool isAnimatedPropertyAttribute = false;
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            isAnimatedPropertyAttribute = accessor.isAnimatedProperty();
        });
        return isAnimatedPropertyAttribute;
    }

    bool isAnimatedStylePropertyAttribute(const QualifiedName& attributeName) const override
    {
        static NeverDestroyed<HashSet<QualifiedName::QualifiedNameImpl*>> animatedStyleAttributes = std::initializer_list<QualifiedName::QualifiedNameImpl*> {
            SVGNames::cxAttr->impl(),
            SVGNames::cyAttr->impl(),
            SVGNames::rAttr->impl(),
            SVGNames::rxAttr->impl(),
            SVGNames::ryAttr->impl(),
            SVGNames::heightAttr->impl(),
            SVGNames::widthAttr->impl(),
            SVGNames::xAttr->impl(),
            SVGNames::yAttr->impl()
        };
        return isAnimatedLengthAttribute(attributeName) && animatedStyleAttributes.get().contains(attributeName.impl());
    }

    RefPtr<SVGAttributeAnimator> createAnimator(SVGElement& element, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const override
    {
        RefPtr<SVGAttributeAnimator> animator;
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            animator = accessor.createAnimator(owner(element), attributeName, animationMode, calcMode, isAccumulated, isAdditive);
        });
        return animator;
    }

    void appendAnimatedInstance(SVGElement& element, const QualifiedName& attributeName, SVGAttributeAnimator& animator) const override
    {
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            accessor.appendAnimatedInstance(owner(element), animator);
        });
    }

private:
    friend class NeverDestroyed<SVGPropertyOwnerRegistry>;
    SVGPropertyOwnerRegistry() = default;

    const OwnerType& CLANG_POINTER_CONVERSION owner(const SVGElement& element) const
    {
        if constexpr (std::is_same_v<OwnerType, SVGElement>)
            return assertIsOwner(element);
        else
            return assertIsOwner(downcast<OwnerType>(element));
    }

    OwnerType& CLANG_POINTER_CONVERSION owner(SVGElement& element) const
    {
        if constexpr (std::is_same_v<OwnerType, SVGElement>)
            return assertIsOwner(element);
        else
            return assertIsOwner(downcast<OwnerType>(element));
    }

    template<typename T>
    T& CLANG_POINTER_CONVERSION assertIsOwner(T& owner) const
    {
        ASSERT(&owner.propertyRegistry() == this);
        return owner;
    }
};

} // namespace WebCore
