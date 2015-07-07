/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK) && ENABLE(DATACUE_VALUE)
#include "SerializedPlatformRepresentationMac.h"

#import "JSDOMBinding.h"
#import "SoftLinking.h"
#import <objc/runtime.h>
#import <runtime/ArrayBuffer.h>
#import <runtime/JSArrayBuffer.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/NSString.h>
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <wtf/text/Base64.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)
SOFT_LINK(CoreMedia, CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator))

typedef AVMetadataItem AVMetadataItemType;
SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVMetadataItem)
#define AVMetadataItem getAVMetadataItemClass()


namespace WebCore {

#if JSC_OBJC_API_ENABLED
static JSValue *jsValueWithDataInContext(NSData *, JSContext *);
static JSValue *jsValueWithArrayInContext(NSArray *, JSContext *);
static JSValue *jsValueWithDictionaryInContext(NSDictionary *, JSContext *);
static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItemType *, JSContext *);
static JSValue *jsValueWithValueInContext(id, JSContext *);
#endif

SerializedPlatformRepresentationMac::SerializedPlatformRepresentationMac(id nativeValue)
    : SerializedPlatformRepresentation()
    , m_nativeValue(nativeValue)
{
}

SerializedPlatformRepresentationMac::~SerializedPlatformRepresentationMac()
{
}

PassRef<SerializedPlatformRepresentation> SerializedPlatformRepresentationMac::create(id nativeValue)
{
    return adoptRef(*new SerializedPlatformRepresentationMac(nativeValue));
}

PassRefPtr<ArrayBuffer> SerializedPlatformRepresentationMac::data() const
{
    return nullptr;
}

JSC::JSValue SerializedPlatformRepresentationMac::deserialize(JSC::ExecState* exec) const
{
#if JSC_OBJC_API_ENABLED
    if (!m_nativeValue)
        return JSC::jsNull();

    JSGlobalContextRef jsGlobalContextRef = toGlobalRef(exec->lexicalGlobalObject()->globalExec());
    JSContext *jsContext = [JSContext contextWithJSGlobalContextRef:jsGlobalContextRef];
    JSValue *serializedValue = jsValueWithValueInContext(m_nativeValue.get(), jsContext);

    return toJS(exec, [serializedValue JSValueRef]);
#else
    UNUSED_PARAM(exec);
    return JSC::jsNull();
#endif
}

bool SerializedPlatformRepresentationMac::isEqual(const SerializedPlatformRepresentation& other) const
{
    if (other.platformType() != SerializedPlatformRepresentation::ObjC)
        return false;

    const SerializedPlatformRepresentationMac* otherObjC = toSerializedPlatformRepresentationMac(&other);

    if (!m_nativeValue || !otherObjC->nativeValue())
        return false;

    return [m_nativeValue.get() isEqual:otherObjC->nativeValue()];
}

SerializedPlatformRepresentationMac* toSerializedPlatformRepresentationMac(SerializedPlatformRepresentation* rep)
{
    return const_cast<SerializedPlatformRepresentationMac*>(toSerializedPlatformRepresentationMac(const_cast<const SerializedPlatformRepresentation*>(rep)));
}

const SerializedPlatformRepresentationMac* toSerializedPlatformRepresentationMac(const SerializedPlatformRepresentation* rep)
{
    ASSERT_WITH_SECURITY_IMPLICATION(rep->platformType() == SerializedPlatformRepresentation::ObjC);
    return static_cast<const SerializedPlatformRepresentationMac*>(rep);
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
    
    if ([value isKindOfClass:[AVMetadataItem class]])
        return jsValueWithAVMetadataItemInContext(value, context);
    
    return nil;
}

static JSValue *jsValueWithDataInContext(NSData *data, JSContext *context)
{
    RefPtr<ArrayBuffer> dataArray = ArrayBuffer::create([data bytes], [data length]);

    JSC::ExecState* exec = toJS([context JSGlobalContextRef]);
    JSC::JSValue array = toJS(exec, JSC::jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()), dataArray.get());

    return [JSValue valueWithJSValueRef:toRef(exec, array) inContext:context];
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

        JSStringRef name = JSStringCreateWithCFString((CFStringRef)key);
        JSObjectSetProperty([context JSGlobalContextRef], resultObject, name, [value JSValueRef], 0, &exception);
        if (exception)
            continue;
    }

    return result;
}

static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItemType *item, JSContext *context)
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

    return jsValueWithDictionaryInContext(dictionary, context);
}
#endif

} // namespace WebCore

#endif
