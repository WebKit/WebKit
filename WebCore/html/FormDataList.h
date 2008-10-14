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

#ifndef FormDataList_h
#define FormDataList_h

#include "CString.h"
#include "File.h"
#include "TextEncoding.h"

namespace WebCore {

class FormDataList {
public:
    FormDataList(const TextEncoding&);

    void appendData(const String& key, const String& value)
        { appendString(key); appendString(value); }
    void appendData(const String& key, const CString& value)
        { appendString(key); appendString(value); }
    void appendData(const String& key, int value)
        { appendString(key); appendString(String::number(value)); }
    void appendFile(const String& key, PassRefPtr<File> file)
        { appendString(key); m_list.append(file); }

    class Item {
    public:
        Item() { }
        Item(const CString& data) : m_data(data) { }
        Item(PassRefPtr<File> file) : m_file(file) { }

        const CString& data() const { return m_data; }
        File* file() const { return m_file.get(); }

    private:
        CString m_data;
        RefPtr<File> m_file;
    };

    const Vector<Item>& list() const { return m_list; }

private:
    void appendString(const CString&);
    void appendString(const String&);

    TextEncoding m_encoding;
    Vector<Item> m_list;
};

} // namespace WebCore

#endif // FormDataList_h
