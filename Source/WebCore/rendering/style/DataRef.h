/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include <wtf/Ref.h>

namespace WebCore {

template <typename T> class DataRef {
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

    bool operator!=(const DataRef& other) const
    {
        return !(*this == other);
    }

private:
    Ref<T> m_data;
};

} // namespace WebCore
