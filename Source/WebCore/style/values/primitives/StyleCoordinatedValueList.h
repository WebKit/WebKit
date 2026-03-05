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

#include <WebCore/StyleCoordinatedValueListValue.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/IndexedRange.h>

namespace WebCore {
namespace Style {

// https://drafts.csswg.org/css-values-4/#coordinating-list-property
template<typename T>
struct CoordinatedValueList {
    using Container = Vector<T, 0, CrashOnOverflow, 0>;
    using value_type = T;

    template<CSSValueID valueID>
    CoordinatedValueList(Constant<valueID> keyword)
        : CoordinatedValueList { Container { value_type { keyword } } }
    {
    }

    CoordinatedValueList(T&& value)
        : CoordinatedValueList { Container { WTF::move(value) } }
    {
    }

    CoordinatedValueList(std::initializer_list<T>&& values)
        : CoordinatedValueList { Container { WTF::move(values) } }
    {
    }

    CoordinatedValueList& access() LIFETIME_BOUND
    {
        if (!m_data->hasOneRef()) {
            Ref oldData = m_data;
            m_data = oldData->clone();
        }
        return *this;
    }

    // Must be called after modifying the list or its values, before any of the used*() functions are called.
    void prepareForUse();

    void append(T&& value) { m_data->container.append(WTF::move(value)); }

    value_type& usedFirst() LIFETIME_BOUND { return m_data->container.first(); }
    const value_type& usedFirst() const LIFETIME_BOUND { return m_data->container.first(); }
    value_type& computedFirst() LIFETIME_BOUND { return m_data->container.first(); }
    const value_type& computedFirst() const LIFETIME_BOUND { return m_data->container.first(); }

    value_type& usedLast() LIFETIME_BOUND { return m_data->container[m_data->usedLength - 1]; }
    const value_type& usedLast() const LIFETIME_BOUND { return m_data->container[m_data->usedLength - 1]; }
    value_type& computedLast() LIFETIME_BOUND { return m_data->container.last(); }
    const value_type& computedLast() const LIFETIME_BOUND { return m_data->container.last(); }

    T& operator[](size_t i) LIFETIME_BOUND { return m_data->container[i]; }
    const T& operator[](size_t i) const LIFETIME_BOUND { return m_data->container[i]; }

    unsigned usedLength() const { return m_data->usedLength; }
    unsigned computedLength() const { return m_data->container.size(); }

    std::span<const T> usedValues() const LIFETIME_BOUND { return m_data->container.span().first(m_data->usedLength); }
    std::span<const T> computedValues() const LIFETIME_BOUND { return m_data->container.span(); }

    bool isInitial() const { return m_data->container.size() == 1 && m_data->container[0].isInitial() && allNonBasePropertiesAreUnsetOrFilled(m_data->container[0]); }

    bool operator==(const CoordinatedValueList& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

private:
    CoordinatedValueList(Container&& container)
        : m_data { Data::create(WTF::move(container)) }
    {
    }

    void removeEmptyValues();
    void fillUnsetProperties();
    void computeUsedLength();

    class Data : public RefCounted<Data> {
    public:
        static Ref<Data> create(Container&& container) { return adoptRef(*new Data(WTF::move(container))); }

        Ref<Data> clone() const
        {
            return adoptRef(*new Data(
                container.template map<Container>([](auto& item) {
                    return value_type::clone(item);
                })
            ));
        }

        bool operator==(const Data& other) const
        {
            return container == other.container;
        }

        Data(Container&& container) : container { WTF::move(container) } { RELEASE_ASSERT(this->container.size() > 0); }

        Container container;
        unsigned usedLength { 1 };
    };

    Ref<Data> m_data;
};

template<typename T>
void CoordinatedValueList<T>::prepareForUse()
{
    removeEmptyValues();
    fillUnsetProperties();
    computeUsedLength();
}

template<typename T>
void CoordinatedValueList<T>::removeEmptyValues()
{
    for (auto [i, value] : indexedRange(m_data->container)) {
        if (i > 0) {
            if (allPropertiesAreUnsetOrFilled(value)) {
                m_data->container.resize(i);
                break;
            }
        }
    }
}

template<typename T>
void CoordinatedValueList<T>::fillUnsetProperties()
{
    auto fillUnsetProperty = [this]<auto propertyID>() {
        using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;

        size_t i = 0;
        for (; i < m_data->container.size() && PropertyAccessor { m_data->container[i] }.isSet(); ++i) { }
        if (i) {
            for (size_t j = 0; i < m_data->container.size(); ++i, ++j)
                PropertyAccessor { m_data->container[i] }.fill(typename PropertyAccessor::Type { PropertyAccessor { m_data->container[j] }.get() });
        }
    };

    eachCoordinatedValueListProperties<T>(fillUnsetProperty);
}

template<typename T>
void CoordinatedValueList<T>::computeUsedLength()
{
    // The length of the coordinated value list is determined by the number of items
    // specified in one particular coordinating list property, the coordinating list
    // base property. (In the case of backgrounds, this is the background-image property.).

    using BasePropertyAccessor = CoordinatedValueListPropertyConstAccessor<T::baseProperty.value>;

    unsigned result = 0;
    for (auto& value : m_data->container) {
        if (!BasePropertyAccessor { value }.isSet())
            break;
        ++result;
    }
    m_data->usedLength = std::max(1u, result);
}

// MARK: - Logging

template<typename T>
TextStream& operator<<(TextStream& ts, const CoordinatedValueList<T>& value)
{
    logForCSSOnRangeLike(ts, value.computedValues(), ", "_s);
    return ts;
}

} // namespace Style
} // namespace WebCore
