/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include <wtf/text/AtomString.h>

#include <mutex>
#include <wtf/text/IntegerToStringConversion.h>

#include <wtf/dtoa.h>

namespace WTF {

template<AtomString::CaseConvertType type>
ALWAYS_INLINE AtomString AtomString::convertASCIICase() const
{
    StringImpl* impl = this->impl();
    if (UNLIKELY(!impl))
        return nullAtom();

    // Convert short strings without allocating a new StringImpl, since
    // there's a good chance these strings are already in the atom
    // string table and so no memory allocation will be required.
    unsigned length;
    const unsigned localBufferSize = 100;
    if (impl->is8Bit() && (length = impl->length()) <= localBufferSize) {
        const LChar* characters = impl->characters8();
        unsigned failingIndex;
        for (unsigned i = 0; i < length; ++i) {
            if (type == CaseConvertType::Lower ? UNLIKELY(isASCIIUpper(characters[i])) : LIKELY(isASCIILower(characters[i]))) {
                failingIndex = i;
                goto SlowPath;
            }
        }
        return *this;
SlowPath:
        LChar localBuffer[localBufferSize];
        for (unsigned i = 0; i < failingIndex; ++i)
            localBuffer[i] = characters[i];
        for (unsigned i = failingIndex; i < length; ++i)
            localBuffer[i] = type == CaseConvertType::Lower ? toASCIILower(characters[i]) : toASCIIUpper(characters[i]);
        return AtomString(localBuffer, length);
    }

    Ref<StringImpl> convertedString = type == CaseConvertType::Lower ? impl->convertToASCIILowercase() : impl->convertToASCIIUppercase();
    if (LIKELY(convertedString.ptr() == impl))
        return *this;

    AtomString result;
    result.m_string = AtomStringImpl::add(convertedString.ptr());
    return result;
}

AtomString AtomString::convertToASCIILowercase() const
{
    return convertASCIICase<CaseConvertType::Lower>();
}

AtomString AtomString::convertToASCIIUppercase() const
{
    return convertASCIICase<CaseConvertType::Upper>();
}

AtomString AtomString::number(int number)
{
    return numberToStringSigned<AtomString>(number);
}

AtomString AtomString::number(unsigned number)
{
    return numberToStringUnsigned<AtomString>(number);
}

AtomString AtomString::number(unsigned long number)
{
    return numberToStringUnsigned<AtomString>(number);
}

AtomString AtomString::number(unsigned long long number)
{
    return numberToStringUnsigned<AtomString>(number);
}

AtomString AtomString::number(float number)
{
    NumberToStringBuffer buffer;
    return numberToString(number, buffer);
}

AtomString AtomString::number(double number)
{
    NumberToStringBuffer buffer;
    return numberToString(number, buffer);
}

AtomString AtomString::fromUTF8Internal(const char* start, const char* end)
{
    ASSERT(start);

    // Caller needs to handle empty string.
    ASSERT(!end || end > start);
    ASSERT(end || start[0]);

    return AtomStringImpl::addUTF8(start, end ? end : start + std::strlen(start));
}

#ifndef NDEBUG

void AtomString::show() const
{
    m_string.show();
}

#endif

WTF_EXPORT_PRIVATE LazyNeverDestroyed<const AtomString> nullAtomData;
WTF_EXPORT_PRIVATE LazyNeverDestroyed<const AtomString> emptyAtomData;
WTF_EXPORT_PRIVATE MainThreadLazyNeverDestroyed<const AtomString> starAtomData;
WTF_EXPORT_PRIVATE MainThreadLazyNeverDestroyed<const AtomString> xmlAtomData;
WTF_EXPORT_PRIVATE MainThreadLazyNeverDestroyed<const AtomString> xmlnsAtomData;

void AtomString::init()
{
    static std::once_flag initializeKey;
    std::call_once(initializeKey, [] {
        // Initialization is not thread safe, so this function must be called from the main thread first.
        ASSERT(isUIThread());

        nullAtomData.construct();
        emptyAtomData.construct("");

        // When starting WebThread via StartWebThread function, we have special period between the prologue of StartWebThread and spawning WebThread actually.
        // In this period, `isMainThread()` returns false even if this is called on the main thread since WebThread is now enabled and we are not taking a WebThread lock.
        // This causes assertion hits in MainThreadLazyNeverDestroyed initialization only in WebThread platforms.
        // We bypass this by using constructWithoutAccessCheck, which intentionally skips `isMainThread()` check for construction.
        // In non WebThread environment, we do not lose the assertion coverage since we already have ASSERT(isUIThread()). And ASSERT(isUIThread()) ensures that this
        // is called in system main thread in WebThread platforms.
        starAtomData.constructWithoutAccessCheck("*", AtomString::ConstructFromLiteral);
        xmlAtomData.constructWithoutAccessCheck("xml", AtomString::ConstructFromLiteral);
        xmlnsAtomData.constructWithoutAccessCheck("xmlns", AtomString::ConstructFromLiteral);
    });
}

} // namespace WTF
