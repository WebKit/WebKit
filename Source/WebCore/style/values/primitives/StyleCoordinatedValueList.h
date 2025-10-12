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

namespace WebCore {
namespace Style {

// https://www.w3.org/TR/css-values-4/#coordinating-list-property
template<typename T>
struct CoordinatedValueList {
    using Container = Vector<T, 0, CrashOnOverflow, 0>;
    using value_type = T;
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using reverse_iterator = typename Container::reverse_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;

    CoordinatedValueList(CSS::Keyword::None)
        : CoordinatedValueList { Container { value_type { } } }
    {
    }

    CoordinatedValueList(T&& value)
        : CoordinatedValueList { Container { WTFMove(value) } }
    {
    }

    CoordinatedValueList(std::initializer_list<T>&& values)
        : CoordinatedValueList { Container { WTFMove(values) } }
    {
    }

    CoordinatedValueList(Container&& container)
        : m_data { Data::create(WTFMove(container)) }
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

    void append(T&& value) { m_data->value.append(WTFMove(value)); }
    void resize(size_t n) { m_data->value.resize(n); }
    void removeAt(size_t i) { m_data->value.removeAt(i); }

    iterator begin() LIFETIME_BOUND { return m_data->value.begin(); }
    iterator end() LIFETIME_BOUND { return m_data->value.end(); }
    const_iterator begin() const LIFETIME_BOUND { return m_data->value.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_data->value.end(); }

    reverse_iterator rbegin() LIFETIME_BOUND { return m_data->value.rbegin(); }
    reverse_iterator rend() LIFETIME_BOUND { return m_data->value.rend(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_data->value.rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_data->value.rend(); }

    value_type& first() LIFETIME_BOUND { return m_data->value.first(); }
    const value_type& first() const LIFETIME_BOUND { return m_data->value.first(); }

    value_type& last() LIFETIME_BOUND { return m_data->value.last(); }
    const value_type& last() const LIFETIME_BOUND { return m_data->value.last(); }

    value_type& operator[](size_t i) LIFETIME_BOUND { return m_data->value[i]; }
    const value_type& operator[](size_t i) const LIFETIME_BOUND { return m_data->value[i]; }

    unsigned size() const { return m_data->value.size(); }
    bool isEmpty() const { return m_data->value.isEmpty(); }

    bool isNone() const { return m_data->value.isEmpty() || (m_data->value.size() == 1 && m_data->value[0].isEmpty()); }

    void fillUnsetProperties() { T::fillUnsetProperties(*this); }

    bool operator==(const CoordinatedValueList& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

private:
    class Data : public RefCounted<Data> {
    public:
        static Ref<Data> create() { return adoptRef(*new Data); }
        static Ref<Data> create(Container&& value) { return adoptRef(*new Data(WTFMove(value))); }

        Ref<Data> clone() const
        {
            return adoptRef(*new Data(
                value.template map<Container>([](auto& item) {
                    return value_type::clone(item);
                })
            ));
        }

        bool operator==(const Data& other) const
        {
            return value == other.value;
        }

        Data() = default;
        Data(Container&& value) : value { WTFMove(value) } { }

        Container value;
    };

    Ref<Data> m_data;
};

// MARK: - Logging

template<typename T>
TextStream& operator<<(TextStream& ts, const CoordinatedValueList<T>& value)
{
    logForCSSOnRangeLike(ts, value, ", "_s);
    return ts;
}

} // namespace Style
} // namespace WebCore
