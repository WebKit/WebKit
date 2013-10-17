/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2008 Apple Inc. All rights reserved.
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

#ifndef DataRef_h
#define DataRef_h

#include <wtf/PassRef.h>
#include <wtf/Ref.h>

namespace WebCore {

template <typename T> class DataRef {
public:
    DataRef(PassRef<T> data) : m_data(std::move(data)) { }
    DataRef(const DataRef& other) : m_data(const_cast<T&>(other.m_data.get())) { }
    DataRef& operator=(const DataRef& other) { m_data = const_cast<T&>(other.m_data.get()); return *this; }

    const T* get() const { return &m_data.get(); }

    const T& operator*() const { return *get(); }
    const T* operator->() const { return get(); }

    T* access()
    {
        if (!m_data->hasOneRef())
            m_data = m_data->copy();
        return &m_data.get();
    }

    bool operator==(const DataRef<T>& o) const
    {
        return &m_data.get() == &o.m_data.get() || m_data.get() == o.m_data.get();
    }
    
    bool operator!=(const DataRef<T>& o) const
    {
        return &m_data.get() != &o.m_data.get() && m_data.get() != o.m_data.get();
    }

private:
    Ref<T> m_data;
};

} // namespace WebCore

#endif // DataRef_h
