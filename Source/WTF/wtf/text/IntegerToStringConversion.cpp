/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
#include "IntegerToStringConversion.h"

#include <wtf/text/StringImpl.h>

namespace WTF {

template<typename T> struct UnsignedIntegerTrait;

template<> struct UnsignedIntegerTrait<short> {
    typedef unsigned short Type;
};
template<> struct UnsignedIntegerTrait<int> {
    typedef unsigned int Type;
};
template<> struct UnsignedIntegerTrait<long> {
    typedef unsigned long Type;
};
template<> struct UnsignedIntegerTrait<long long> {
    typedef unsigned long long Type;
};

template<typename SignedIntegerType>
static PassRefPtr<StringImpl> numberToStringImplSigned(SignedIntegerType integer)
{
    LChar buf[1 + sizeof(SignedIntegerType) * 3];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    bool negative = false;
    typename UnsignedIntegerTrait<SignedIntegerType>::Type unsignedInteger;
    if (integer < 0) {
        negative = true;
        unsignedInteger = -integer;
    } else
        unsignedInteger = integer;

    do {
        *--p = static_cast<LChar>((unsignedInteger % 10) + '0');
        unsignedInteger /= 10;
    } while (unsignedInteger);

    if (negative)
        *--p = '-';

    return StringImpl::create(p, static_cast<unsigned>(end - p));
}

PassRefPtr<StringImpl> numberToStringImpl(int i)
{
    return numberToStringImplSigned(i);
}

PassRefPtr<StringImpl> numberToStringImpl(long i)
{
    return numberToStringImplSigned(i);
}

PassRefPtr<StringImpl> numberToStringImpl(long long i)
{
    return numberToStringImplSigned(i);
}


template<typename UnsignedIntegerType>
PassRefPtr<StringImpl> numberToStringImplUnsigned(UnsignedIntegerType integer)
{
    LChar buf[sizeof(UnsignedIntegerType) * 3];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    do {
        *--p = static_cast<LChar>((integer % 10) + '0');
        integer /= 10;
    } while (integer);
    return StringImpl::create(p, static_cast<unsigned>(end - p));
}

PassRefPtr<StringImpl> numberToStringImpl(unsigned u)
{
    return numberToStringImplUnsigned(u);
}

PassRefPtr<StringImpl> numberToStringImpl(unsigned long u)
{
    return numberToStringImplUnsigned(u);
}

PassRefPtr<StringImpl> numberToStringImpl(unsigned long long u)
{
    return numberToStringImplUnsigned(u);
}

} // namespace WTF
