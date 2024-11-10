/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "SerializedPlatformDataCueValue.h"

#import <AVFoundation/AVMetadataItem.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

SerializedPlatformDataCueValue::SerializedPlatformDataCueValue(AVMetadataItem *item)
{
    if (!item)
        return;

    m_data = Data { };

    auto dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    NSDictionary *extras = [item extraAttributes];

    for (id key in [extras keyEnumerator]) {
        if (![key isKindOfClass:[NSString class]])
            continue;
        NSString *value = [extras objectForKey:key];
        if (![value isKindOfClass:NSString.class])
            continue;
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
        } else {
            if (![value length])
                continue;
            keyString = [key lowercaseString];
        }

        if ([keyString isEqualToString:@"type"])
            m_data->type = value;
        else
            m_data->otherAttributes.add(keyString, value);
    }

    if ([item.key isKindOfClass:NSString.class])
        m_data->key = (NSString *)item.key;

    if (item.locale)
        m_data->locale = item.locale;

    if (auto *str = dynamic_objc_cast<NSString>(item.value))
        m_data->value = str;
    else if (auto *data = dynamic_objc_cast<NSData>(item.value))
        m_data->value = data;
    else if (auto *date = dynamic_objc_cast<NSDate>(item.value))
        m_data->value = date;
    else if (auto *number = dynamic_objc_cast<NSNumber>(item.value))
        m_data->value = number;
}

RetainPtr<NSDictionary> SerializedPlatformDataCueValue::toNSDictionary() const
{
    if (!m_data)
        return nullptr;

    auto dictionary = adoptNS([NSMutableDictionary new]);

    if (!m_data->type.isNull())
        [dictionary setObject:m_data->type forKey:@"type"];

    for (auto& pair : m_data->otherAttributes)
        [dictionary setObject:pair.value forKey:pair.key];

    if (m_data->locale)
        [dictionary setObject:m_data->locale.get() forKey:@"locale"];

    if (!m_data->key.isNull())
        [dictionary setObject:m_data->key forKey:@"key"];

    WTF::switchOn(m_data->value, [] (std::nullptr_t) {
    }, [&] (RetainPtr<NSString> string) {
        [dictionary setValue:string.get() forKey:@"data"];
    }, [&] (RetainPtr<NSNumber> number) {
        [dictionary setValue:number.get() forKey:@"data"];
    }, [&] (RetainPtr<NSData> data) {
        [dictionary setValue:data.get() forKey:@"data"];
    }, [&] (RetainPtr<NSDate> date) {
        [dictionary setValue:date.get() forKey:@"data"];
    });

    return dictionary;
}

bool SerializedPlatformDataCueValue::operator==(const SerializedPlatformDataCueValue& other) const
{
    if (!m_data || !other.m_data)
        return false;

    return *m_data == *other.m_data;
}

bool SerializedPlatformDataCueValue::Data::operator==(const Data& other) const
{
    return type == other.type
        && otherAttributes == other.otherAttributes
        && [locale isEqual:other.locale.get()]
        && key == other.key
        && value.index() == other.value.index()
        && WTF::switchOn(value, [] (std::nullptr_t) {
            return true;
        }, [&] (RetainPtr<NSString> string) {
            return !![string isEqual:std::get<RetainPtr<NSString>>(other.value).get()];
        }, [&] (RetainPtr<NSNumber> number) {
            return !![number isEqual:std::get<RetainPtr<NSNumber>>(other.value).get()];
        }, [&] (RetainPtr<NSData> data) {
            return !![data isEqual:std::get<RetainPtr<NSData>>(other.value).get()];
        }, [&] (RetainPtr<NSDate> date) {
            return !![date isEqual:std::get<RetainPtr<NSDate>>(other.value).get()];
        });
}

}
