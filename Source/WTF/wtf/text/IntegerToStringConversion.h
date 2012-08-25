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

#ifndef IntegerToStringConversion_h
#define IntegerToStringConversion_h

#include "StringImpl.h"

namespace WTF {
WTF_EXPORT_STRING_API PassRefPtr<StringImpl> numberToStringImpl(int);
WTF_EXPORT_STRING_API PassRefPtr<StringImpl> numberToStringImpl(long);
WTF_EXPORT_STRING_API PassRefPtr<StringImpl> numberToStringImpl(long long);

WTF_EXPORT_STRING_API PassRefPtr<StringImpl> numberToStringImpl(unsigned);
WTF_EXPORT_STRING_API PassRefPtr<StringImpl> numberToStringImpl(unsigned long);
WTF_EXPORT_STRING_API PassRefPtr<StringImpl> numberToStringImpl(unsigned long long);
}

#endif // IntegerToStringConversion_h
