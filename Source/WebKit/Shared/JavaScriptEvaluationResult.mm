/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "JavaScriptEvaluationResult.h"

#import "APINodeInfo.h"
#import "_WKNodeInfoInternal.h"

namespace WebKit {

RetainPtr<id> JavaScriptEvaluationResult::toID(Value&& root)
{
    return WTF::switchOn(WTFMove(root), [] (EmptyType type) -> RetainPtr<id> {
        switch (type) {
        case EmptyType::Null:
            return NSNull.null;
        case EmptyType::Undefined:
            break;
        }
        return nullptr;
    }, [] (bool value) -> RetainPtr<id> {
        return value ? @YES : @NO;
    }, [] (double value) -> RetainPtr<id> {
        return adoptNS([[NSNumber alloc] initWithDouble:value]);
    }, [] (String&& value) -> RetainPtr<id> {
        return value.createNSString();
    }, [] (Seconds value) -> RetainPtr<id> {
        return [NSDate dateWithTimeIntervalSince1970:value.seconds()];
    }, [&] (Vector<JSObjectID>&& vector) -> RetainPtr<id> {
        RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:vector.size()]);
        m_nsArrays.append({ WTFMove(vector), array });
        return array;
    }, [&] (HashMap<JSObjectID, JSObjectID>&& map) -> RetainPtr<id> {
        RetainPtr dictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
        m_nsDictionaries.append({ WTFMove(map), dictionary });
        return dictionary;
    }, [] (NodeInfo&& nodeInfo) -> RetainPtr<id> {
        return wrapper(API::NodeInfo::create(WTFMove(nodeInfo)).get());
    });
}

RetainPtr<id> JavaScriptEvaluationResult::toID()
{
    for (auto [identifier, variant] : std::exchange(m_map, { }))
        m_instantiatedNSObjects.add(identifier, toID(WTFMove(variant)));
    for (auto [vector, array] : std::exchange(m_nsArrays, { })) {
        for (auto identifier : vector) {
            if (RetainPtr element = m_instantiatedNSObjects.get(identifier))
                [array addObject:element.get()];
        }
    }
    for (auto [map, dictionary] : std::exchange(m_nsDictionaries, { })) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            RetainPtr key = m_instantiatedNSObjects.get(keyIdentifier);
            if (!key)
                continue;
            RetainPtr value = m_instantiatedNSObjects.get(valueIdentifier);
            if (!value)
                continue;
            [dictionary setObject:value.get() forKey:key.get()];
        }
    }
    return std::exchange(m_instantiatedNSObjects, { }).take(m_root);
}

auto JavaScriptEvaluationResult::toValue(id object) -> Value
{
    if (!object)
        return EmptyType::Undefined;

    if ([object isKindOfClass:NSNull.class])
        return EmptyType::Null;

    if ([object isKindOfClass:NSNumber.class]) {
        if (CFNumberGetType((CFNumberRef)object) == kCFNumberCharType) {
            if ([object isEqual:@YES])
                return true;
            if ([object isEqual:@NO])
                return false;
        }
        return [(NSNumber *)object doubleValue];
    }

    if (auto* nsString = dynamic_objc_cast<NSString>(object))
        return String(nsString);

    if ([object isKindOfClass:NSDate.class])
        return Seconds([(NSDate *)object timeIntervalSince1970]);

    if ([object isKindOfClass:NSArray.class]) {
        Vector<JSObjectID> vector;
        for (id element : (NSArray *)object)
            vector.append(addObjectToMap(element));
        return { WTFMove(vector) };
    }

    if ([object isKindOfClass:NSDictionary.class]) {
        HashMap<JSObjectID, JSObjectID> map;
        [(NSDictionary *)object enumerateKeysAndObjectsUsingBlock:[&](id key, id value, BOOL *) {
            map.add(addObjectToMap(key), addObjectToMap(value));
        }];
        return { WTFMove(map) };
    }

    // This object has been null checked and went through isSerializable which only supports these types.
    ASSERT_NOT_REACHED();
    return EmptyType::Undefined;
}

JSObjectID JavaScriptEvaluationResult::addObjectToMap(id object)
{
    if (!object) {
        if (!m_nullObjectID) {
            m_nullObjectID = JSObjectID::generate();
            m_map.add(*m_nullObjectID, Value { EmptyType::Undefined });
        }
        return *m_nullObjectID;
    }

    auto it = m_objectsInMap.find(object);
    if (it != m_objectsInMap.end())
        return it->value;

    auto identifier = JSObjectID::generate();
    m_objectsInMap.set(object, identifier);
    m_map.add(identifier, toValue(object));
    return identifier;
}

static bool isSerializable(id argument)
{
    if (!argument)
        return true;

    if ([argument isKindOfClass:[NSString class]] || [argument isKindOfClass:[NSNumber class]] || [argument isKindOfClass:[NSDate class]] || [argument isKindOfClass:[NSNull class]])
        return true;

    if ([argument isKindOfClass:[NSArray class]]) {
        __block BOOL valid = true;

        [argument enumerateObjectsUsingBlock:^(id object, NSUInteger, BOOL *stop) {
            if (!isSerializable(object)) {
                valid = false;
                *stop = YES;
            }
        }];

        return valid;
    }

    if ([argument isKindOfClass:[NSDictionary class]]) {
        __block bool valid = true;

        [argument enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
            if (!isSerializable(key) || !isSerializable(value)) {
                valid = false;
                *stop = YES;
            }
        }];

        return valid;
    }

    return false;
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(id object)
{
    if (object && !isSerializable(object))
        return std::nullopt;
    return JavaScriptEvaluationResult { object };
}

JavaScriptEvaluationResult::JavaScriptEvaluationResult(id object)
    : m_root(addObjectToMap(object))
{
    m_objectsInMap.clear();
    m_nullObjectID = std::nullopt;
}

}
