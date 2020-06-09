/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import <wtf/text/WTFString.h>

#import <CoreFoundation/CFString.h>

namespace WTF {

String::String(NSString *str)
{
    if (!str)
        return;

    CFIndex size = CFStringGetLength((__bridge CFStringRef)str);
    if (!size)
        m_impl = StringImpl::empty();
    else {
        Vector<LChar, 1024> lcharBuffer(size);
        CFIndex usedBufLen;
        CFIndex convertedsize = CFStringGetBytes((__bridge CFStringRef)str, CFRangeMake(0, size), kCFStringEncodingISOLatin1, 0, false, lcharBuffer.data(), size, &usedBufLen);
        if ((convertedsize == size) && (usedBufLen == size)) {
            m_impl = StringImpl::create(lcharBuffer.data(), size);
            return;
        }

        Vector<UChar, 1024> ucharBuffer(size);
        CFStringGetCharacters((__bridge CFStringRef)str, CFRangeMake(0, size), reinterpret_cast<UniChar*>(ucharBuffer.data()));
        m_impl = StringImpl::create(ucharBuffer.data(), size);
    }
}

RetainPtr<id> makeNSArrayElement(const String& vectorElement)
{
    return adoptNS((__bridge_transfer id)vectorElement.createCFString().leakRef());
}

Optional<String> makeVectorElement(const String*, id arrayElement)
{
    if (![arrayElement isKindOfClass:NSString.class])
        return WTF::nullopt;
    return { { arrayElement } };
}

}
