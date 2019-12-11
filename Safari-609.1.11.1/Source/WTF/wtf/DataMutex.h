/*
 * Copyright (C) 2019 Igalia, S.L.
 * Copyright (C) 2019 Metrological Group B.V.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <wtf/Lock.h>

namespace WTF {

template<typename T>
class DataMutex {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DataMutex);
public:
    template<typename ...Args>
    explicit DataMutex(Args&&... args)
        : m_data(std::forward<Args>(args)...)
    { }

    class LockedWrapper {
    public:
        explicit LockedWrapper(DataMutex& dataMutex)
            : m_mutex(dataMutex.m_mutex)
            , m_lockHolder(dataMutex.m_mutex)
            , m_data(dataMutex.m_data)
        { }

        T* operator->()
        {
            return &m_data;
        }

        T& operator*()
        {
            return m_data;
        }

        Lock& mutex()
        {
            return m_mutex;
        }

        LockHolder& lockHolder()
        {
            return m_lockHolder;
        }

    private:
        Lock& m_mutex;
        LockHolder m_lockHolder;
        T& m_data;
    };

private:
    Lock m_mutex;
    T m_data;
};

} // namespace WTF
