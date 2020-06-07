/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "SerializedPlatformDataCueMac.h"

#if ENABLE(VIDEO) && ENABLE(DATACUE_VALUE)

#import "JSDOMConvertBufferSource.h"
#import <AVFoundation/AVMetadataItem.h>
#import <Foundation/NSString.h>
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ArrayBuffer.h>
#import <JavaScriptCore/JSArrayBuffer.h>
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <objc/runtime.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

#if JSC_OBJC_API_ENABLED
static JSValue *jsValueWithDataInContext(NSData *, JSContext *);
static JSValue *jsValueWithArrayInContext(NSArray *, JSContext *);
static JSValue *jsValueWithDictionaryInContext(NSDictionary *, JSContext *);
static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItem *, JSContext *);
static JSValue *jsValueWithValueInContext(id, JSContext *);
static NSDictionary *NSDictionaryWithAVMetadataItem(AVMetadataItem *);
#endif

Ref<SerializedPlatformDataCue> SerializedPlatformDataCue::create(SerializedPlatformDataCueValue&& value)
{
    return adoptRef(*new SerializedPlatformDataCueMac(WTFMove(value)));
}

SerializedPlatformDataCueMac::SerializedPlatformDataCueMac(SerializedPlatformDataCueValue&& value)
    : SerializedPlatformDataCue()
    , m_nativeValue(value.nativeValue())
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(value.platformType() == SerializedPlatformDataCueValue::PlatformType::ObjC);
}

RefPtr<ArrayBuffer> SerializedPlatformDataCueMac::data() const
{
    return nullptr;
}

JSC::JSValue SerializedPlatformDataCueMac::deserialize(JSC::JSGlobalObject* lexicalGlobalObject) const
{
#if JSC_OBJC_API_ENABLED
    if (!m_nativeValue)
        return JSC::jsNull();

    JSGlobalContextRef jsGlobalContextRef = toGlobalRef(lexicalGlobalObject);
    JSContext *jsContext = [JSContext contextWithJSGlobalContextRef:jsGlobalContextRef];
    JSValue *serializedValue = jsValueWithValueInContext(m_nativeValue.get(), jsContext);

    return toJS(lexicalGlobalObject, [serializedValue JSValueRef]);
#else
    UNUSED_PARAM(lexicalGlobalObject);
    return JSC::jsNull();
#endif
}

bool SerializedPlatformDataCueMac::isEqual(const SerializedPlatformDataCue& other) const
{
    if (other.platformType() != PlatformType::ObjC)
        return false;

    const SerializedPlatformDataCueMac* otherObjC = toSerializedPlatformDataCueMac(&other);

    if (!m_nativeValue || !otherObjC->nativeValue())
        return false;

    return [m_nativeValue.get() isEqual:otherObjC->nativeValue()];
}

SerializedPlatformDataCueMac* toSerializedPlatformDataCueMac(SerializedPlatformDataCue* rep)
{
    return const_cast<SerializedPlatformDataCueMac*>(toSerializedPlatformDataCueMac(const_cast<const SerializedPlatformDataCue*>(rep)));
}

const SerializedPlatformDataCueMac* toSerializedPlatformDataCueMac(const SerializedPlatformDataCue* rep)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(rep->platformType() == SerializedPlatformDataCue::PlatformType::ObjC);
    return static_cast<const SerializedPlatformDataCueMac*>(rep);
}

NSArray *SerializedPlatformDataCueMac::allowedClassesForNativeValues()
{
    static NeverDestroyed<RetainPtr<NSArray>> allowedClasses(@[ [NSString class], [NSNumber class], [NSLocale class], [NSDictionary class], [NSArray class], [NSData class] ]);
    return allowedClasses.get().get();
}

SerializedPlatformDataCueValue SerializedPlatformDataCueMac::encodableValue() const
{
    if ([m_nativeValue.get() isKindOfClass:PAL::getAVMetadataItemClass()])
        return { SerializedPlatformDataCueValue::PlatformType::ObjC, NSDictionaryWithAVMetadataItem(m_nativeValue.get()) };

    return { SerializedPlatformDataCueValue::PlatformType::ObjC, m_nativeValue.get() };
}

#if JSC_OBJC_API_ENABLED
static JSValue *jsValueWithValueInContext(id value, JSContext *context)
{
    if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]])
        return [JSValue valueWithObject:value inContext:context];

    if ([value isKindOfClass:[NSLocale class]])
        return [JSValue valueWithObject:[value localeIdentifier] inContext:context];

    if ([value isKindOfClass:[NSDictionary class]])
        return jsValueWithDictionaryInContext(value, context);

    if ([value isKindOfClass:[NSArray class]])
        return jsValueWithArrayInContext(value, context);

    if ([value isKindOfClass:[NSData class]])
        return jsValueWithDataInContext(value, context);

    if ([value isKindOfClass:PAL::getAVMetadataItemClass()])
        return jsValueWithAVMetadataItemInContext(value, context);

    return nil;
}

static JSValue *jsValueWithDataInContext(NSData *data, JSContext *context)
{
    auto dataArray = ArrayBuffer::tryCreate([data bytes], [data length]);

    auto* lexicalGlobalObject = toJS([context JSGlobalContextRef]);
    JSC::JSValue array = toJS(lexicalGlobalObject, JSC::jsCast<JSDOMGlobalObject*>(lexicalGlobalObject), dataArray.get());

    return [JSValue valueWithJSValueRef:toRef(lexicalGlobalObject, array) inContext:context];
}

static JSValue *jsValueWithArrayInContext(NSArray *array, JSContext *context)
{
    JSValueRef exception = 0;
    JSValue *result = [JSValue valueWithNewArrayInContext:context];
    JSObjectRef resultObject = JSValueToObject([context JSGlobalContextRef], [result JSValueRef], &exception);
    if (exception)
        return [JSValue valueWithUndefinedInContext:context];

    NSUInteger count = [array count];
    for (NSUInteger i = 0; i < count; ++i) {
        JSValue *value = jsValueWithValueInContext([array objectAtIndex:i], context);
        if (!value)
            continue;

        JSObjectSetPropertyAtIndex([context JSGlobalContextRef], resultObject, (unsigned)i, [value JSValueRef], &exception);
        if (exception)
            continue;
    }

    return result;
}

static JSValue *jsValueWithDictionaryInContext(NSDictionary *dictionary, JSContext *context)
{
    JSValueRef exception = 0;
    JSValue *result = [JSValue valueWithNewObjectInContext:context];
    JSObjectRef resultObject = JSValueToObject([context JSGlobalContextRef], [result JSValueRef], &exception);
    if (exception)
        return [JSValue valueWithUndefinedInContext:context];

    for (id key in [dictionary keyEnumerator]) {
        if (![key isKindOfClass:[NSString class]])
            continue;

        JSValue *value = jsValueWithValueInContext([dictionary objectForKey:key], context);
        if (!value)
            continue;

        auto name = OpaqueJSString::tryCreate(key);
        JSObjectSetProperty([context JSGlobalContextRef], resultObject, name.get(), [value JSValueRef], 0, &exception);
        if (exception)
            continue;
    }

    return result;
}

static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItem *item, JSContext *context)
{
    return jsValueWithDictionaryInContext(NSDictionaryWithAVMetadataItem(item), context);
}

static NSDictionary *NSDictionaryWithAVMetadataItem(AVMetadataItem *item)
{
    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];

    NSDictionary *extras = [item extraAttributes];
    for (id key in [extras keyEnumerator]) {
        if (![key isKindOfClass:[NSString class]])
            continue;
        id value = [extras objectForKey:key];
        NSString *keyString = key;

        if ([key isEqualToString:@"MIMEtype"])
            keyString = @"type";
        else if ([key isEqualToString:@"dataTypeNamespace"] || [key isEqualToString:@"pictureType"])
            continue;
        else if ([key isEqualToString:@"dataType"]) {
            id dataTypeNamespace = [extras objectForKey:@"dataTypeNamespace"];
            if (!dataTypeNamespace || ![dataTypeNamespace isKindOfClass:[NSString class]] || ![dataTypeNamespace isEqualToString:@"org.iana.media-type"])
                continue;
            keyString = @"type";
        } else if ([value isKindOfClass:[NSString class]]) {
            if (![value length])
                continue;
            keyString = [key lowercaseString];
        }

        [dictionary setObject:value forKey:keyString];
    }

    if (item.key)
        [dictionary setObject:item.key forKey:@"key"];

    if (item.locale)
        [dictionary setObject:item.locale forKey:@"locale"];

    if (item.value)
        [dictionary setObject:item.value forKey:@"data"];

    return dictionary;
}
#endif

} // namespace WebCore

#endif
