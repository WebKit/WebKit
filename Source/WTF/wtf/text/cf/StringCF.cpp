/*
 * Copyright (C) 2006-2022 Apple Inc.
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
#include <wtf/text/WTFString.h>

#if USE(CF)

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringBuffer.h>

namespace WTF {

String::String(CFStringRef str)
{
    if (!str)
        return;

    CFIndex size = CFStringGetLength(str);
    if (!size) {
        m_impl = StringImpl::empty();
        return;
    }

    {
        StringBuffer<LChar> buffer(size);
        CFIndex usedBufLen;
        CFIndex convertedSize = CFStringGetBytes(str, CFRangeMake(0, size), kCFStringEncodingISOLatin1, 0, false, buffer.characters(), size, &usedBufLen);
        if (convertedSize == size && usedBufLen == size) {
            m_impl = StringImpl::adopt(WTFMove(buffer));
            return;
        }
    }

    StringBuffer<UChar> ucharBuffer(size);
    CFStringGetCharacters(str, CFRangeMake(0, size), reinterpret_cast<UniChar *>(ucharBuffer.characters()));
    m_impl = StringImpl::adopt(WTFMove(ucharBuffer));
}

RetainPtr<CFStringRef> String::createCFString() const
{
    if (!m_impl)
        return CFSTR("");

    return m_impl->createCFString();
}

RetainPtr<CFStringRef> makeCFArrayElement(const String& vectorElement)
{
    return vectorElement.createCFString();
}

std::optional<String> makeVectorElement(const String*, CFStringRef cfString)
{
    if (cfString)
        return { { cfString } };
    return std::nullopt;
}

}

#endif // USE(CF)
