/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "File.h"
#include "TextEncoding.h"
#include <wtf/Forward.h>
#include <wtf/Variant.h>

namespace WebCore {

class FormDataList {
public:
    struct Item {
        using Data = Variant<RefPtr<File>, String>;

        String name;
        Data data;
    };

    void appendData(const String& name, const String& value);
    void appendData(const String& name, int value);
    void appendBlob(const String& name, Ref<Blob>&&, const String& filename = { });
    void setData(const String& name, const String& value);
    void setBlob(const String& name, Ref<Blob>&&, const String& filename = { });

    const Vector<Item>& items() const { return m_items; }
    const TextEncoding& encoding() const { return m_encoding; }

    CString normalizeString(const String&) const;

protected:
    FormDataList(const TextEncoding&);

    Item createFileEntry(const String& name, Ref<Blob>&&, const String& filename);
    void set(const String& name, Item&&);

    TextEncoding m_encoding;
    Vector<Item> m_items;
};

} // namespace WebCore
