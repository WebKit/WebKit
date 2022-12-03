/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <wtf/RetainPtr.h>
#import <wtf/UUID.h>
#import <wtf/WallTime.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSSet;
OBJC_CLASS NSString;
OBJC_CLASS NSUUID;

namespace WebKit {

template<typename T> T *filterObjects(T *container, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value));
template<> NSArray *filterObjects<NSArray>(NSArray *, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value));
template<> NSDictionary *filterObjects<NSDictionary>(NSDictionary *, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value));
template<> NSSet *filterObjects<NSSet>(NSSet *, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value));

template<typename T>
T *filterObjects(const RetainPtr<T>& container, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
    return filterObjects<T>(container.get(), block);
}

template<typename T> T *mapObjects(T *container, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value));
template<> NSArray *mapObjects<NSArray>(NSArray *, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value));
template<> NSDictionary *mapObjects<NSDictionary>(NSDictionary *, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value));
template<> NSSet *mapObjects<NSSet>(NSSet *, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value));

template<typename T>
T *mapObjects(const RetainPtr<T>& container, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
    return mapObjects<T>(container.get(), block);
}

template<typename T>
T *objectForKey(NSDictionary *dictionary, id key, bool returningNilIfEmpty = true, Class containingObjectsOfClass = Nil)
{
    ASSERT(returningNilIfEmpty);
    ASSERT(!containingObjectsOfClass);
    // Specialized implementations in CocoaHelpers.mm handle returningNilIfEmpty and containingObjectsOfClass for different Foundation types.
    return dynamic_objc_cast<T>(dictionary[key]);
}

template<typename T>
T *objectForKey(const RetainPtr<NSDictionary>& dictionary, id key, bool returningNilIfEmpty = true, Class containingObjectsOfClass = Nil)
{
    return objectForKey<T>(dictionary.get(), key, returningNilIfEmpty, containingObjectsOfClass);
}

NSString *escapeCharactersInString(NSString *, NSString *charactersToEscape);

NSDate *toAPI(const WallTime&);
WallTime toImpl(NSDate *);

} // namespace WebKit
