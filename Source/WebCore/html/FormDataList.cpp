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

#include "config.h"
#include "FormDataList.h"

#include "LineEnding.h"
#include <wtf/text/CString.h>

namespace WebCore {

FormDataList::FormDataList(const TextEncoding& encoding)
    : m_encoding(encoding)
{
}

CString FormDataList::normalizeString(const String& value) const
{
    return normalizeLineEndingsToCRLF(m_encoding.encode(value, EntitiesForUnencodables));
}

// https://xhr.spec.whatwg.org/#create-an-entry
auto FormDataList::createFileEntry(const String& name, Ref<Blob>&& blob, const String& filename) -> Item
{
    if (!blob->isFile())
        return { name, File::create(blob.get(), filename.isNull() ? ASCIILiteral("blob") : filename) };
    
    if (!filename.isNull())
        return { name, File::create(downcast<File>(blob.get()), filename) };

    return { name, static_reference_cast<File>(WTFMove(blob)) };
}

void FormDataList::appendData(const String& name, const String& value)
{
    m_items.append({ name, value });
}

void FormDataList::appendData(const String& name, int value)
{
    m_items.append({ name, String::number(value) });
}

void FormDataList::appendBlob(const String& name, Ref<Blob>&& blob, const String& filename)
{
    m_items.append(createFileEntry(name, WTFMove(blob), filename));
}

void FormDataList::set(const String& name, Item&& item)
{
    std::optional<size_t> initialMatchLocation;

    // Find location of the first item with a matching name.
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (name == m_items[i].name) {
            initialMatchLocation = i;
            break;
        }
    }

    if (initialMatchLocation) {
        m_items[*initialMatchLocation] = WTFMove(item);

        m_items.removeAllMatching([&name] (const auto& item) {
            return item.name == name;
        }, *initialMatchLocation + 1);
        return;
    }

    m_items.append(WTFMove(item));
}

void FormDataList::setData(const String& name, const String& value)
{
    set(name, { name, value });
}

void FormDataList::setBlob(const String& name, Ref<Blob>&& blob, const String& filename)
{
    set(name, createFileEntry(name, WTFMove(blob), filename));
}

}
