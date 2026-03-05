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
#import "CoreIPCCFDictionary.h"

#if USE(CF)

#import "CoreIPCBoolean.h"
#import "CoreIPCCFArray.h"
#import "CoreIPCCFCharacterSet.h"
#import "CoreIPCCFType.h"
#import "CoreIPCCFURL.h"
#import "CoreIPCTypes.h"
#include <optional>

namespace WebKit {

static std::optional<CoreIPCCFDictionary::KeyType> keyVariantFromCFType(CFTypeRef cfType)
{
    switch (IPC::typeFromCFTypeRef(cfType)) {
    case IPC::CFType::CFArray:
        return CoreIPCCFArray((CFArrayRef)cfType);
    case IPC::CFType::CFBoolean:
        return CoreIPCBoolean((CFBooleanRef)cfType);
    case IPC::CFType::CFCharacterSet:
        return CoreIPCCFCharacterSet((CFCharacterSetRef)cfType);
    case IPC::CFType::CFData:
        return CoreIPCData((CFDataRef)cfType);
    case IPC::CFType::CFDate:
        return CoreIPCDate((CFDateRef)cfType);
    case IPC::CFType::CFDictionary:
        return CoreIPCCFDictionary((CFDictionaryRef)cfType);
    case IPC::CFType::CFNull:
        return CoreIPCNull((CFNullRef)cfType);
    case IPC::CFType::CFNumber:
        return CoreIPCNumber((CFNumberRef)cfType);
    case IPC::CFType::CFString:
        return CoreIPCString((CFStringRef)cfType);
    case IPC::CFType::CFURL:
        return CoreIPCCFURL((CFURLRef)cfType);
    case IPC::CFType::Nullptr:
    case IPC::CFType::SecCertificate:
    case IPC::CFType::SecAccessControl:
    case IPC::CFType::SecTrust:
    case IPC::CFType::CGColorSpace:
    case IPC::CFType::CGColor:
    case IPC::CFType::Unknown:
        return std::nullopt;
    }
}

static RetainPtr<CFTypeRef> keyToCFType(const CoreIPCCFDictionary::KeyType& keyType)
{
    return WTF::switchOn(keyType, [] (const CoreIPCCFArray& array) -> RetainPtr<CFTypeRef> {
        return array.createCFArray();
    }, [] (const CoreIPCBoolean& boolean) -> RetainPtr<CFTypeRef> {
        return boolean.createBoolean();
    }, [] (const CoreIPCCFCharacterSet& characterSet) -> RetainPtr<CFTypeRef> {
        return characterSet.toCF();
    }, [] (const CoreIPCData& data) -> RetainPtr<CFTypeRef> {
        return data.data();
    }, [] (const CoreIPCDate& date) -> RetainPtr<CFTypeRef> {
        return date.createCFDate();
    }, [] (const CoreIPCCFDictionary& dictionary) -> RetainPtr<CFTypeRef> {
        return dictionary.createCFDictionary();
    }, [] (const CoreIPCNull& nullRef) -> RetainPtr<CFTypeRef> {
        return nullRef.toCFObject();
    }, [] (const CoreIPCNumber& number) -> RetainPtr<CFTypeRef> {
        return number.createCFNumber();
    }, [] (const CoreIPCString& string) -> RetainPtr<CFTypeRef> {
        return (CFStringRef)string.toID().get();
    }, [] (const CoreIPCCFURL& url) -> RetainPtr<CFTypeRef> {
        return url.createCFURL();
    });
}

CoreIPCCFDictionary::CoreIPCCFDictionary(std::unique_ptr<KeyValueVector>&& vector)
    : m_vector(WTF::move(vector)) { }

CoreIPCCFDictionary::CoreIPCCFDictionary(CoreIPCCFDictionary&&) = default;

CoreIPCCFDictionary::~CoreIPCCFDictionary() = default;

CoreIPCCFDictionary::CoreIPCCFDictionary(CFDictionaryRef dictionary)
{
    if (!dictionary)
        return;
    m_vector = makeUnique<KeyValueVector>();
    m_vector->reserveInitialCapacity(CFDictionaryGetCount(dictionary));
    [(__bridge NSDictionary *)dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
        auto keyType = keyVariantFromCFType(key);
        if (!keyType)
            return;
        if (IPC::typeFromCFTypeRef(value) == IPC::CFType::Unknown)
            return;
        m_vector->append({ WTF::move(*keyType), CoreIPCCFType(value) });
    }];
}

RetainPtr<CFDictionaryRef> CoreIPCCFDictionary::createCFDictionary() const
{
    if (!m_vector)
        return nil;

    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_vector->size()]);
    for (auto& pair : *m_vector) {
        RetainPtr<id> key = (__bridge id)keyToCFType(pair.key).get();
        auto value = pair.value.toID();
        if (key && value)
            [result setObject:value.get() forKey:key.get()];
    }
    return (__bridge CFDictionaryRef)result.get();
}

} // namespace WebKit

#endif // USE(CF)
