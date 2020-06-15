/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DOMMimeTypeArray.h"

#include "DOMMimeType.h"
#include "Navigator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMMimeTypeArray);

Ref<DOMMimeTypeArray> DOMMimeTypeArray::create(Navigator& navigator, Vector<Ref<DOMMimeType>>&& types)
{
    return adoptRef(*new DOMMimeTypeArray(navigator, WTFMove(types)));
}

DOMMimeTypeArray::DOMMimeTypeArray(Navigator& navigator, Vector<Ref<DOMMimeType>>&& types)
    : m_navigator(makeWeakPtr(navigator))
    , m_types(WTFMove(types))
{
}

DOMMimeTypeArray::~DOMMimeTypeArray() = default;

unsigned DOMMimeTypeArray::length() const
{
    return m_types.size();
}

RefPtr<DOMMimeType> DOMMimeTypeArray::item(unsigned index)
{
    if (index >= m_types.size())
        return nullptr;
    return m_types[index].ptr();
}

RefPtr<DOMMimeType> DOMMimeTypeArray::namedItem(const AtomString& propertyName)
{
    for (auto& type : m_types) {
        if (type->type() == propertyName)
            return type.ptr();
    }
    return nullptr;
}

Vector<AtomString> DOMMimeTypeArray::supportedPropertyNames()
{
    Vector<AtomString> result;
    result.reserveInitialCapacity(m_types.size());
    for (auto& type : m_types)
        result.uncheckedAppend(type->type());
    return result;
}

} // namespace WebCore
