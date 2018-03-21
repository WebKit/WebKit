/*
 * Copyright (C) 2018 Igalia S.L.
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

#include <glib.h>
#include <wtf/FastMalloc.h>

namespace JSC {

class JSCGLibWrapperObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    JSCGLibWrapperObject(gpointer object, GDestroyNotify destroyFunction)
        : m_object(object)
        , m_destroyFunction(destroyFunction)
    {
    }

    ~JSCGLibWrapperObject()
    {
        if (m_destroyFunction)
            m_destroyFunction(m_object);
    }

    gpointer object() const { return m_object; }

private:
    gpointer m_object { nullptr };
    GDestroyNotify m_destroyFunction { nullptr };
};

} // namespace JSC
