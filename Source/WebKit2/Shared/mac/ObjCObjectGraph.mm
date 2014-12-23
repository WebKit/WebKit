/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ObjCObjectGraph.h"

namespace WebKit {

template<typename T> T* dynamic_objc_cast(id object)
{
    if ([object isKindOfClass:[T class]])
        return (T *)object;

    return nil;
}

static bool shouldTransform(id object, const ObjCObjectGraph::Transformer& transformer)
{
    if (NSArray *array = dynamic_objc_cast<NSArray>(object)) {
        for (id element in array) {
            if (shouldTransform(element, transformer))
                return true;
        }
    }

    if (NSDictionary *dictionary = dynamic_objc_cast<NSDictionary>(object)) {
        bool result = false;
        [dictionary enumerateKeysAndObjectsUsingBlock:[&transformer, &result](id key, id object, BOOL* stop) {
            if (shouldTransform(object, transformer)) {
                result = true;
                *stop = YES;
            }
        }];

        return result;
    }

    return transformer.shouldTransformObjectOfType([object class]);
}

RetainPtr<id> ObjCObjectGraph::transform(id object, const Transformer& transformer)
{
    if (!object)
        return nullptr;

    if (!shouldTransform(object, transformer))
        return object;

    if (NSArray *array = dynamic_objc_cast<NSArray>(object)) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:array.count]);
        for (id element in array)
            [result addObject:transform(element, transformer).get()];

        return result;
    }

    if (NSDictionary *dictionary = dynamic_objc_cast<NSDictionary>(object)) {
        auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:dictionary.count]);
        [dictionary enumerateKeysAndObjectsUsingBlock:[&result, &transformer](id key, id object, BOOL*) {
            [result setObject:transformer.transformObject(object).get() forKey:key];
        }];

        return result;
    }

    return transformer.transformObject(object);
}

}
