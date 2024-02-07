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

#import "CoreIPCCFType.h"
#import "CoreIPCTypes.h"

namespace WebKit {

CoreIPCCFDictionary::CoreIPCCFDictionary(std::unique_ptr<KeyValueVector>&& vector)
    : m_vector(WTFMove(vector)) { }

CoreIPCCFDictionary::CoreIPCCFDictionary(CoreIPCCFDictionary&&) = default;

CoreIPCCFDictionary::~CoreIPCCFDictionary() = default;

CoreIPCCFDictionary::CoreIPCCFDictionary(CFDictionaryRef dictionary)
{
    if (!dictionary)
        return;
    m_vector = makeUnique<KeyValueVector>();
    m_vector->reserveInitialCapacity(CFDictionaryGetCount(dictionary));
    [(__bridge NSDictionary *)dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
        if (IPC::typeFromCFTypeRef(key) == IPC::CFType::Unknown)
            return;
        if (IPC::typeFromCFTypeRef(value) == IPC::CFType::Unknown)
            return;
        m_vector->append({ CoreIPCCFType(key), CoreIPCCFType(value) });
    }];
}

RetainPtr<CFDictionaryRef> CoreIPCCFDictionary::createCFDictionary() const
{
    if (!m_vector)
        return nil;

    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_vector->size()]);
    for (auto& pair : *m_vector) {
        auto key = pair.key.toID();
        auto value = pair.value.toID();
        if (key && value)
            [result setObject:value.get() forKey:key.get()];
    }
    return (__bridge CFDictionaryRef)result.get();
}

} // namespace WebKit

#endif // USE(CF)
