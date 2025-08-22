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

#import "_WKJSHandleInternal.h"
#import "_WKSerializedNodeInternal.h"

namespace WebKit {

class JavaScriptEvaluationResult::ObjCExtractor {
public:
    Map takeMap() { return WTFMove(m_map); }
    JSObjectID addObjectToMap(id);
private:
    Value toValue(id);

    Map m_map;
    HashMap<RetainPtr<id>, JSObjectID> m_objectsInMap;
};

class JavaScriptEvaluationResult::ObjCInserter {
public:
    using Dictionaries = Vector<std::pair<ObjectMap, RetainPtr<NSMutableDictionary>>>;
    using Arrays = Vector<std::pair<Vector<JSObjectID>, RetainPtr<NSMutableArray>>>;
    RetainPtr<id> toID(Value&&);
    Dictionaries takeDictionaries() { return WTFMove(m_dictionaries); }
    Arrays takeArrays() { return WTFMove(m_arrays); }
private:
    Dictionaries m_dictionaries;
    Arrays m_arrays;
};

RetainPtr<id> JavaScriptEvaluationResult::ObjCInserter::toID(Value&& root)
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
        m_arrays.append({ WTFMove(vector), array });
        return array;
    }, [&] (ObjectMap&& map) -> RetainPtr<id> {
        RetainPtr dictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
        m_dictionaries.append({ WTFMove(map), dictionary });
        return dictionary;
    }, [] (UniqueRef<JSHandleInfo>&& info) -> RetainPtr<id> {
        return wrapper(API::JSHandle::create(WTFMove(info.get())));
    }, [] (UniqueRef<WebCore::SerializedNode>&& serializedNode) -> RetainPtr<id> {
        return wrapper(API::SerializedNode::create(WTFMove(serializedNode.get())).get());
    });
}

RetainPtr<id> JavaScriptEvaluationResult::toID()
{
    HashMap<JSObjectID, RetainPtr<id>> instantiatedObjects;
    ObjCInserter inserter;

    for (auto&& [identifier, value] : std::exchange(m_map, { }))
        instantiatedObjects.add(identifier, inserter.toID(WTFMove(value)));

    for (auto [vector, array] : inserter.takeArrays()) {
        for (auto identifier : vector) {
            if (RetainPtr element = instantiatedObjects.get(identifier))
                [array addObject:element.get()];
            else
                [array addObject:NSNull.null];
        }
    }

    for (auto [map, dictionary] : inserter.takeDictionaries()) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            RetainPtr key = instantiatedObjects.get(keyIdentifier);
            if (!key)
                continue;
            RetainPtr value = instantiatedObjects.get(valueIdentifier);
            if (!value)
                continue;
            [dictionary setObject:value.get() forKey:key.get()];
        }
    }

    return std::exchange(instantiatedObjects, { }).take(m_root);
}

auto JavaScriptEvaluationResult::ObjCExtractor::toValue(id object) -> Value
{
    ASSERT(object);

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
        ObjectMap map;
        [(NSDictionary *)object enumerateKeysAndObjectsUsingBlock:[&](id key, id value, BOOL *) {
            map.add(addObjectToMap(key), addObjectToMap(value));
        }];
        return { WTFMove(map) };
    }

    if ([object isKindOfClass:_WKSerializedNode.class])
        return makeUniqueRef<WebCore::SerializedNode>(((_WKSerializedNode *)object)->_node->coreSerializedNode());

    if ([object isKindOfClass:_WKJSHandle.class])
        return makeUniqueRef<JSHandleInfo>(((_WKJSHandle *)object)->_ref->info());

    // This object has been null checked and went through isSerializable which only supports these types.
    ASSERT_NOT_REACHED();
    return EmptyType::Undefined;
}

JSObjectID JavaScriptEvaluationResult::ObjCExtractor::addObjectToMap(id object)
{
    ASSERT(object);

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

    if ([argument isKindOfClass:NSString.class]
        || [argument isKindOfClass:NSNumber.class]
        || [argument isKindOfClass:NSDate.class]
        || [argument isKindOfClass:NSNull.class]
        || [argument isKindOfClass:_WKJSHandle.class]
        || [argument isKindOfClass:_WKSerializedNode.class])
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
    if (!object)
        return jsUndefined();

    if (!isSerializable(object))
        return std::nullopt;

    ObjCExtractor extractor;
    auto root = extractor.addObjectToMap(object);
    return JavaScriptEvaluationResult { root, extractor.takeMap() };
}

}
