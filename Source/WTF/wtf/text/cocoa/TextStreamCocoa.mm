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
#import <wtf/text/TextStreamCocoa.h>

#import <objc/runtime.h>
#import <wtf/text/cf/StringConcatenateCF.h>

namespace WTF {

TextStream& TextStream::operator<<(id object)
{
    if (object_isClass(object)) {
        m_text.append(NSStringFromClass(object));
        return *this;
    }

    auto outputArray = [&](NSArray *array) {
        *this << "[";

        for (NSUInteger i = 0; i < array.count; ++i) {
            id item = array[i];
            *this << item;
            if (i < array.count - 1)
                *this << ", ";
        }

        *this << "]";
    };

    if ([object isKindOfClass:[NSArray class]]) {
        outputArray(object);
        return *this;
    }

    auto outputDictionary = [&](NSDictionary *dictionary) {
        *this << "{";
        bool needLeadingComma = false;

        [dictionary enumerateKeysAndObjectsUsingBlock:[&](id key, id value, BOOL *) {
            if (needLeadingComma)
                *this << ", ";
            needLeadingComma = true;

            *this << key;
            *this << ": ";
            *this << value;
        }];

        *this << "}";
    };

    if ([object isKindOfClass:[NSDictionary class]]) {
        outputDictionary(object);
        return *this;
    }

    if ([object conformsToProtocol:@protocol(NSObject)])
        m_text.append([object description]);
    else
        *this << "(id)";

    return *this;
}

TextStream& operator<<(TextStream& ts, CGRect rect)
{
    ts << "{{" << rect.origin.x << ", " << rect.origin.y << "}, {" << rect.size.width << ", " << rect.size.height << "}}";
    return ts;
}

TextStream& operator<<(TextStream& ts, CGSize size)
{
    ts << "{" << size.width << ", " << size.height << "}";
    return ts;
}

TextStream& operator<<(TextStream& ts, CGPoint point)
{
    ts << "{" << point.x << ", " << point.y << "}";
    return ts;
}

} // namespace WTF
