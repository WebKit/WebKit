/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <wtf/HashTraits.h>
#include <wtf/Ref.h>

namespace WTF {

template <typename T>
class DataRef {
public:
    DataRef(Ref<T>&& data)
        : m_data(WTFMove(data))
    {
    }

    DataRef(const DataRef& other)
        : m_data(other.m_data.copyRef())
    {
    }

    DataRef& operator=(const DataRef& other)
    {
        m_data = other.m_data.copyRef();
        return *this;
    }

    DataRef(DataRef&&) = default;
    DataRef& operator=(DataRef&&) = default;

    DataRef replace(DataRef&& other)
    {
        return m_data.replace(WTFMove(other.m_data));
    }

    operator const T&() const
    {
        return m_data;
    }

    const T* ptr() const
    {
        return m_data.ptr();
    }

    const T& get() const
    {
        return m_data;
    }

    const T& operator*() const
    {
        return m_data;
    }

    const T* operator->() const
    {
        return m_data.ptr();
    }

    T& access()
    {
        if (!m_data->hasOneRef())
            m_data = m_data->copy();
        return m_data;
    }

    bool operator==(const DataRef& other) const
    {
        return m_data.ptr() == other.m_data.ptr() || m_data.get() == other.m_data.get();
    }

    DataRef(HashTableDeletedValueType)
        : m_data(HashTableDeletedValue)
    {
    }
    bool isHashTableDeletedValue() const { return m_data.isHashTableDeletedValue(); }

    DataRef(HashTableEmptyValueType)
        : m_data(HashTableEmptyValue)
    {
    }
    bool isHashTableEmptyValue() const { return m_data.isHashTableEmptyValue(); }
    static T* hashTableEmptyValue() { return nullptr; }

private:
    Ref<T> m_data;
};

template<typename T> struct HashTraits<DataRef<T>> : SimpleClassHashTraits<DataRef<T>> {
    static constexpr bool emptyValueIsZero = true;
    static DataRef<T> emptyValue() { return HashTableEmptyValue; }

    template <typename>
    static void constructEmptyValue(DataRef<T>& slot)
    {
        new (NotNull, std::addressof(slot)) DataRef<T>(HashTableEmptyValue);
    }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const DataRef<T>& value) { return value.isHashTableEmptyValue(); }
};

} // namespace WTF

using WTF::DataRef;
