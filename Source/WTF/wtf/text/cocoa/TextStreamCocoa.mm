/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import <wtf/text/TextStream.h>

#import <wtf/text/cf/StringConcatenateCF.h>

namespace WTF {

TextStream& TextStream::operator<<(id object)
{
    if ([object isKindOfClass:[NSArray class]])
        return *this << static_cast<NSArray *>(object);

    if ([object conformsToProtocol:@protocol(NSObject)])
        m_text.append([object description]);
    else
        m_text.append("(id)");
    return *this;
}

TextStream& TextStream::operator<<(NSArray *array)
{
    *this << "[";

    for (NSUInteger i = 0; i < array.count; ++i) {
        id item = array[i];
        *this << item;
        if (i < array.count - 1)
            *this << ", ";
    }

    return *this << "]";
}

}
