/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "SVGValue.h"

namespace WebCore {

class SVGAnimatedType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename PropertyType>
    static std::unique_ptr<SVGAnimatedType> create()
    {
        return std::make_unique<SVGAnimatedType>(SVGPropertyTraits<PropertyType>::initialValue());
    }

    template<typename PropertyType>
    static std::unique_ptr<SVGAnimatedType> create(const PropertyType& property)
    {
        return std::make_unique<SVGAnimatedType>(property);
    }

    template<typename PropertyType>
    static std::unique_ptr<SVGAnimatedType> create(PropertyType&& property)
    {
        return std::make_unique<SVGAnimatedType>(WTFMove(property));
    }

    template<typename PropertyType1, typename PropertyType2>
    static std::unique_ptr<SVGAnimatedType> create()
    {
        return std::make_unique<SVGAnimatedType>(SVGPropertyTraits<std::pair<PropertyType1, PropertyType2>>::initialValue());
    }

    template<typename PropertyType1, typename PropertyType2>
    static std::unique_ptr<SVGAnimatedType> create(const std::pair<PropertyType1, PropertyType2>& properties)
    {
        return std::make_unique<SVGAnimatedType>(properties);
    }

    template<typename PropertyType1, typename PropertyType2>
    static std::unique_ptr<SVGAnimatedType> create(std::pair<PropertyType1, PropertyType2>&& properties)
    {
        return std::make_unique<SVGAnimatedType>(WTFMove(properties));
    }

    template<typename PropertyType>
    SVGAnimatedType(const PropertyType& property)
        : m_value(std::make_unique<PropertyType>(property).release())
    {
    }

    template<typename PropertyType>
    SVGAnimatedType(PropertyType&& property)
        : m_value(std::make_unique<PropertyType>(WTFMove(property)).release())
    {
    }

    template<typename PropertyType1, typename PropertyType2>
    SVGAnimatedType(const std::pair<PropertyType1, PropertyType2>& properties)
        : m_value(std::make_unique<std::pair<PropertyType1, PropertyType2>>(properties).release())
    {
    }

    template<typename PropertyType1, typename PropertyType2>
    SVGAnimatedType(std::pair<PropertyType1, PropertyType2>&& properties)
        : m_value(std::make_unique<std::pair<PropertyType1, PropertyType2>>(WTFMove(properties)).release())
    {
    }

    ~SVGAnimatedType()
    {
        WTF::visit([](auto& value) {
            delete value;
        }, m_value);
    }

    template <class PropertyType>
    const PropertyType& as() const
    {
        ASSERT(WTF::holds_alternative<PropertyType*>(m_value));
        return *WTF::get<PropertyType*>(m_value);
    }

    template <class PropertyType>
    PropertyType& as()
    {
        ASSERT(WTF::holds_alternative<PropertyType*>(m_value));
        return *WTF::get<PropertyType*>(m_value);
    }

    AnimatedPropertyType type() const
    {
        ASSERT(m_value.index() >= AnimatedPropertyTypeMin && m_value.index() < AnimatedPropertyTypeMax);
        return static_cast<AnimatedPropertyType>(m_value.index());
    }

    String valueAsString() const
    {
        return WTF::visit([](auto& value) {
            using PropertyType = std::decay_t<decltype(*value)>;
            return SVGPropertyTraits<PropertyType>::toString(*value);
        }, m_value);
    }

    bool setValueAsString(const QualifiedName& attrName, const String& string)
    {
        return WTF::visit([&](auto& value) {
            using PropertyType = std::decay_t<decltype(*value)>;
            if (auto result = SVGPropertyTraits<PropertyType>::parse(attrName, string)) {
                *value = *result;
                return true;
            }
            return false;
        }, m_value);
    }

    static bool supportsAnimVal(AnimatedPropertyType type)
    {
        // AnimatedColor is only used for CSS property animations.
        return type != AnimatedUnknown && type != AnimatedColor;
    }

private:
    SVGValueVariant m_value;
};

} // namespace WebCore
