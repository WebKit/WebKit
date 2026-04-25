/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Style {

// Utilities and macros for implementing coordinating list property groups.
// https://drafts.csswg.org/css-values-4/#coordinating-list-property

template<typename T> concept CoordinatedValueListValue = requires(T value) {
    { T::computedValueUsesUsedValues } -> std::same_as<const bool&>;
    { T::baseProperty.value } -> std::same_as<const CSSPropertyID&>;
    { value.clone(std::declval<const T&>()) } -> std::same_as<T>;
    { value.isInitial() } -> std::same_as<bool>;
};

enum class CoordinatedValueListPropertyState : uint8_t {
    Unset,
    Set,
    Filled,
};

template<CSSPropertyID> struct CoordinatedValueListPropertyAccessor;
template<CSSPropertyID> struct CoordinatedValueListPropertyConstAccessor;

#define DECLARE_COORDINATED_VALUE_LIST_PROPERTY(ownerType, property, type, lowercaseName, uppercaseName) \
    PropertyNameConstant<CSSProperty##property>{},
\

#define INTERNAL_DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
        static type initial() { return ownerType::initial##uppercaseName(); } \
        bool isUnset() const { return value.is##uppercaseName##Unset(); } \
        bool isSet() const { return value.is##uppercaseName##Set(); } \
        bool isFilled() const { return value.is##uppercaseName##Filled(); } \
        CoordinatedValueListPropertyState state() const { return value.lowercaseName##State(); } \
\

#define DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE(ownerType, property, type, lowercaseName, uppercaseName) \
    template<> struct CoordinatedValueListPropertyAccessor<CSSProperty##property> { \
        using Type = type; \
        \
        ownerType& value; \
        \
        const type& get() const { return value.lowercaseName(); } \
        void set(type&& lowercaseName) { value.set##uppercaseName(WTF::move(lowercaseName)); } \
        void fill(type&& lowercaseName) { value.fill##uppercaseName(WTF::move(lowercaseName)); } \
        void clear() { value.clear##uppercaseName(); } \
        \
        INTERNAL_DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
        \
        bool operator==(const CoordinatedValueListPropertyAccessor<CSSProperty##property>& other) const \
        { \
            return get() == other.get() && state() == other.state(); \
        } \
    }; \
    \
    template<> struct CoordinatedValueListPropertyConstAccessor<CSSProperty##property> { \
        using Type = type; \
        \
        const ownerType& value; \
        \
        const type& get() const { return value.lowercaseName(); } \
        \
        INTERNAL_DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
        \
        bool operator==(const CoordinatedValueListPropertyConstAccessor<CSSProperty##property>& other) const \
        { \
            return get() == other.get() && state() == other.state(); \
        } \
    }; \
\

#define DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_VALUE(ownerType, property, type, lowercaseName, uppercaseName) \
    template<> struct CoordinatedValueListPropertyAccessor<CSSProperty##property> { \
        using Type = type; \
        \
        ownerType& value; \
        \
        type get() const { return value.lowercaseName(); } \
        void set(type lowercaseName) { value.set##uppercaseName(lowercaseName); } \
        void fill(type lowercaseName) { value.fill##uppercaseName(lowercaseName); } \
        void clear() { value.clear##uppercaseName(); } \
        \
        INTERNAL_DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
        \
        bool operator==(const CoordinatedValueListPropertyAccessor<CSSProperty##property>& other) const \
        { \
            return get() == other.get() && state() == other.state(); \
        } \
    }; \
    \
    template<> struct CoordinatedValueListPropertyConstAccessor<CSSProperty##property> { \
        using Type = type; \
        \
        const ownerType& value; \
        \
        type get() const { return value.lowercaseName(); } \
        \
        INTERNAL_DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
        \
        bool operator==(const CoordinatedValueListPropertyConstAccessor<CSSProperty##property>& other) const \
        { \
            return get() == other.get() && state() == other.state(); \
        } \
    }; \
\

#define DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_ENUM(ownerType, property, type, lowercaseName, uppercaseName) \
    DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_VALUE(ownerType, property, type, lowercaseName, uppercaseName) \
\

#define DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_SHORTHAND(ownerType, property, type, lowercaseName, uppercaseName) \
    template<> struct CoordinatedValueListPropertyAccessor<CSSProperty##property> { \
        using Type = type; \
        \
        ownerType& value; \
        \
        type get() const { return value.lowercaseName(); } \
        void set(type&& lowercaseName) { value.set##uppercaseName(WTF::move(lowercaseName)); } \
        void fill(type&& lowercaseName) { value.fill##uppercaseName(WTF::move(lowercaseName)); } \
        void clear() { value.clear##uppercaseName(); } \
        \
        static type initial() { return ownerType::initial##uppercaseName(); } \
        bool isUnset() const { return value.is##uppercaseName##Unset(); } \
        bool isSet() const { return value.is##uppercaseName##Set(); } \
        bool isFilled() const { return value.is##uppercaseName##Filled(); } \
    }; \
    \
    template<> struct CoordinatedValueListPropertyConstAccessor<CSSProperty##property> { \
        using Type = type; \
        \
        const ownerType& value; \
        \
        type get() const { return value.lowercaseName(); } \
        \
        static type initial() { return ownerType::initial##uppercaseName(); } \
        bool isUnset() const { return value.is##uppercaseName##Unset(); } \
        bool isSet() const { return value.is##uppercaseName##Set(); } \
        bool isFilled() const { return value.is##uppercaseName##Filled(); } \
    }; \
\

#define DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS(ownerType, property, type, lowercaseName, uppercaseName) \
    PREFERRED_TYPE(CoordinatedValueListPropertyState) unsigned m_##lowercaseName##State : 2 { static_cast<unsigned>(CoordinatedValueListPropertyState::Unset) }; \
\

#define INTERNAL_DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
    CoordinatedValueListPropertyState lowercaseName##State() const \
    { \
        return static_cast<CoordinatedValueListPropertyState>(data().m_##lowercaseName##State); \
    } \
    bool is##uppercaseName##Unset() const \
    { \
        return lowercaseName##State() == CoordinatedValueListPropertyState::Unset; \
    } \
    bool is##uppercaseName##Set() const \
    { \
        return lowercaseName##State() == CoordinatedValueListPropertyState::Set; \
    } \
    bool is##uppercaseName##Filled() const \
    { \
        return lowercaseName##State() == CoordinatedValueListPropertyState::Filled; \
    } \
\

#define DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE(ownerType, property, type, lowercaseName, uppercaseName) \
    void clear##uppercaseName() \
    { \
        data().m_##lowercaseName = ownerType::initial##uppercaseName(); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Unset); \
    } \
    void set##uppercaseName(type&& lowercaseName) \
    { \
        data().m_##lowercaseName = WTF::move(lowercaseName); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Set); \
    } \
    void fill##uppercaseName(type&& lowercaseName) \
    { \
        data().m_##lowercaseName = WTF::move(lowercaseName); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Filled); \
    } \
    INTERNAL_DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
\

#define DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_VALUE(ownerType, property, type, lowercaseName, uppercaseName) \
    void clear##uppercaseName() \
    { \
        data().m_##lowercaseName = ownerType::initial##uppercaseName(); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Unset); \
    } \
    \
    void set##uppercaseName(type lowercaseName) \
    { \
        data().m_##lowercaseName = lowercaseName; \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Set); \
    } \
    void fill##uppercaseName(type lowercaseName) \
    { \
        data().m_##lowercaseName = lowercaseName; \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Filled); \
    } \
    INTERNAL_DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
\

#define DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_ENUM(ownerType, property, type, lowercaseName, uppercaseName) \
    void clear##uppercaseName() \
    { \
        data().m_##lowercaseName = static_cast<unsigned>(ownerType::initial##uppercaseName()); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Unset); \
    } \
    \
    void set##uppercaseName(type lowercaseName) \
    { \
        data().m_##lowercaseName = static_cast<unsigned>(lowercaseName); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Set); \
    } \
    void fill##uppercaseName(type lowercaseName) \
    { \
        data().m_##lowercaseName = static_cast<unsigned>(lowercaseName); \
        data().m_##lowercaseName##State = static_cast<unsigned>(CoordinatedValueListPropertyState::Filled); \
    } \
    INTERNAL_DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_COMMON(ownerType, property, type, lowercaseName, uppercaseName) \
\

// Applies the provided functor to each property of the CoordinatedValueList value, invoking the functor with each properties CSSPropertyID as the first template argument, and then and'ing the results.
template<CoordinatedValueListValue T, typename F>
constexpr bool allOfCoordinatedValueListProperties(NOESCAPE F&& f)
{
    return WTF::apply([&]<typename... Ts>(const Ts& ...) { return (f.template operator()<Ts::value>() && ...); }, T::properties);
}

// Applies the provided functor to each property of the CoordinatedValueList value, invoking the functor with each properties CSSPropertyID as the first template argument, and then or'ing the results.
template<CoordinatedValueListValue T, typename F>
constexpr bool anyOfCoordinatedValueListProperties(NOESCAPE F&& f)
{
    return WTF::apply([&]<typename... Ts>(const Ts& ...) { return (f.template operator()<Ts::value>() || ...); }, T::properties);
}

// Applies the provided functor to each property of the CoordinatedValueList value, invoking the functor with each properties CSSPropertyID as the first template argument.
template<CoordinatedValueListValue T, typename F>
constexpr void eachCoordinatedValueListProperties(NOESCAPE F&& f)
{
    WTF::apply([&]<typename... Ts>(const Ts& ...) { (f.template operator()<Ts::value>(), ...); }, T::properties);
}

template<CoordinatedValueListValue T>
bool allNonBasePropertiesAreUnsetOrFilled(const T& value)
{
    return allOfCoordinatedValueListProperties<T>([&value]<CSSPropertyID propertyID>() {
        using PropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;

        if constexpr (propertyID == T::baseProperty) {
            return true;
        } else {
            return !PropertyAccessor { value }.isSet();
        }
    });
}

template<CoordinatedValueListValue T>
bool allPropertiesAreUnsetOrFilled(const T& value)
{
    return allOfCoordinatedValueListProperties<T>([&value]<CSSPropertyID propertyID>() {
        using PropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;
        return !PropertyAccessor { value }.isSet();
    });
}

template<CoordinatedValueListValue T>
bool allPropertiesAreSet(const T& value)
{
    return allOfCoordinatedValueListProperties<T>([&value]<CSSPropertyID propertyID>() {
        using PropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;
        return PropertyAccessor { value }.isSet();
    });
}

} // namespace Style
} // namespace WebCore
