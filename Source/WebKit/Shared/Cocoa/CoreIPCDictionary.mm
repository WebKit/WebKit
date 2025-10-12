/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCDictionary.h"
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)

#import "CoreIPCTypes.h"

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoreIPCDictionary);

CoreIPCDictionary::CoreIPCDictionary(NSDictionary *dictionary)
{
    m_keyValuePairs.reserveInitialCapacity(dictionary.count);

    for (id key in dictionary) {
        RetainPtr<id> value = dictionary[key];
        ASSERT(value);

        // Ignore values we don't support.
        ASSERT(IPC::isSerializableValue(key));
        ASSERT(IPC::isSerializableValue(value.get()));
        if (!IPC::isSerializableValue(key) || !IPC::isSerializableValue(value.get()))
            continue;

        m_keyValuePairs.append({ CoreIPCNSCFObject(key), CoreIPCNSCFObject(value.get()) });
    }
}

CoreIPCDictionary::CoreIPCDictionary(const RetainPtr<NSDictionary>& dictionary)
    : CoreIPCDictionary(dictionary.get()) { }

CoreIPCDictionary::CoreIPCDictionary(CoreIPCDictionary&&) = default;

CoreIPCDictionary::~CoreIPCDictionary() = default;

CoreIPCDictionary::CoreIPCDictionary(ValueType&& keyValuePairs)
    : m_keyValuePairs(WTFMove(keyValuePairs)) { }

RetainPtr<id> CoreIPCDictionary::toID() const
{
    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_keyValuePairs.size()]);
    for (auto& keyValuePair : m_keyValuePairs) {
        RetainPtr keyID = keyValuePair.key.toID();
        RetainPtr valueID = keyValuePair.value.toID();
        if (!keyID || !valueID)
            continue;
        [result setObject:valueID.get() forKey:keyID.get()];
    }
    return result;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
