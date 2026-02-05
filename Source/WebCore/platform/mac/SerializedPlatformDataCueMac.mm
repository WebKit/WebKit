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
#import <wtf/cocoa/SpanCocoa.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

#if JSC_OBJC_API_ENABLED
static JSValue *jsValueWithDataInContext(NSData *, JSContext *);
static JSValue *jsValueWithArrayInContext(NSArray *, JSContext *);
static JSValue *jsValueWithDictionaryInContext(NSDictionary *, JSContext *);
static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItem *, JSContext *);
static JSValue *jsValueWithValueInContext(id, JSContext *);
#endif

Ref<SerializedPlatformDataCue> SerializedPlatformDataCue::create(SerializedPlatformDataCueValue&& value)
{
    return adoptRef(*new SerializedPlatformDataCueMac(WTF::move(value)));
}

SerializedPlatformDataCueMac::SerializedPlatformDataCueMac(SerializedPlatformDataCueValue&& value)
    : SerializedPlatformDataCue()
    , m_value(WTF::move(value))
{
}

RefPtr<ArrayBuffer> SerializedPlatformDataCueMac::data() const
{
    return nullptr;
}

JSC::JSValue SerializedPlatformDataCueMac::deserialize(JSC::JSGlobalObject* lexicalGlobalObject) const
{
#if JSC_OBJC_API_ENABLED
    auto dictionary = m_value.toNSDictionary();
    if (!dictionary)
        return JSC::jsNull();

    JSGlobalContextRef jsGlobalContextRef = toGlobalRef(lexicalGlobalObject);
    JSContext *jsContext = [JSContext contextWithJSGlobalContextRef:jsGlobalContextRef];
    RetainPtr serializedValue = jsValueWithValueInContext(dictionary.get(), jsContext);

    return toJS(lexicalGlobalObject, [serializedValue JSValueRef]);
#else
    UNUSED_PARAM(lexicalGlobalObject);
    return JSC::jsNull();
#endif
}

bool SerializedPlatformDataCueMac::isEqual(const SerializedPlatformDataCue& other) const
{
    return m_value == downcast<SerializedPlatformDataCueMac>(other).m_value;
}

const HashSet<RetainPtr<Class>>& SerializedPlatformDataCueMac::allowedClassesForNativeValues()
{
    static NeverDestroyed<HashSet<RetainPtr<Class>>> allowedClasses(HashSet<RetainPtr<Class>> { [NSString class], [NSNumber class], [NSLocale class], [NSDictionary class], [NSArray class], [NSData class] });
    return allowedClasses;
}

SerializedPlatformDataCueValue SerializedPlatformDataCueMac::encodableValue() const
{
    return m_value;
}

#if JSC_OBJC_API_ENABLED
static JSValue *jsValueWithValueInContext(id value, JSContext *context)
{
    if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]])
        return [JSValue valueWithObject:value inContext:context];

    if ([value isKindOfClass:[NSLocale class]])
        return [JSValue valueWithObject:retainPtr([value localeIdentifier]).get() inContext:context];

    if ([value isKindOfClass:[NSDictionary class]])
        return jsValueWithDictionaryInContext(value, context);

    if ([value isKindOfClass:[NSArray class]])
        return jsValueWithArrayInContext(value, context);

    if ([value isKindOfClass:[NSData class]])
        return jsValueWithDataInContext(value, context);

    if ([value isKindOfClass:PAL::getAVMetadataItemClassSingleton()])
        return jsValueWithAVMetadataItemInContext(value, context);

    return nil;
}

static JSValue *jsValueWithDataInContext(NSData *data, JSContext *context)
{
    auto dataArray = ArrayBuffer::tryCreate(span(data));

    auto* lexicalGlobalObject = toJS([context JSGlobalContextRef]);
    // FIXME: It is a layering violation to use the JSDOMGlobalObject type in the platform directory.
    JSC::JSValue array = dataArray ? toJS(lexicalGlobalObject, JSC::jsCast<JSDOMGlobalObject*>(lexicalGlobalObject), *dataArray) : JSC::jsNull();

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
        RetainPtr value = jsValueWithValueInContext(retainPtr([array objectAtIndex:i]).get(), context);
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

        RetainPtr value = jsValueWithValueInContext(retainPtr([dictionary objectForKey:key]).get(), context);
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
    return jsValueWithDictionaryInContext(SerializedPlatformDataCueValue(item).toNSDictionary().get(), context);
}
#endif

} // namespace WebCore

#endif
