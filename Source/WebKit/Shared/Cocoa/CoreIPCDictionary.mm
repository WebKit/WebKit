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

#if PLATFORM(COCOA)

#import "CoreIPCTypes.h"

namespace WebKit {

CoreIPCDictionary::CoreIPCDictionary(NSDictionary *dictionary)
    : m_nsDictionary(dictionary)
{
    m_keyValuePairs.reserveInitialCapacity(dictionary.count);

    for (id key in dictionary) {
        id value = dictionary[key];
        ASSERT(value);

        // Ignore values we don't support.
        ASSERT(IPC::isSerializableValue(key));
        ASSERT(IPC::isSerializableValue(value));
        if (!IPC::isSerializableValue(key) || !IPC::isSerializableValue(value))
            continue;

        m_keyValuePairs.append({ WTF::makeUniqueRef<CoreIPCNSCFObject>(key), WTF::makeUniqueRef<CoreIPCNSCFObject>(value) });
    }
}

bool CoreIPCDictionary::keyHasValueOfType(const String& key, IPC::NSType type) const
{
    createNSDictionaryIfNeeded();
    id object = [m_nsDictionary.get() objectForKey:(NSString *)key];
    if (!object) {
        // Many objects have a required key that sometimes has a missing value, which is okay.
        return true;
    }

    return IPC::typeFromObject(object) == type;
}

bool CoreIPCDictionary::keyIsMissingOrHasValueOfType(const String& key, IPC::NSType type) const
{
    createNSDictionaryIfNeeded();
    id object = [m_nsDictionary.get() objectForKey:(NSString *)key];
    if (!object)
        return true;
    return IPC::typeFromObject(object) == type;
}

bool CoreIPCDictionary::collectionValuesAreOfType(const String& key, IPC::NSType targetType) const
{
    createNSDictionaryIfNeeded();
    NSArray *object = [m_nsDictionary.get() objectForKey:(NSString *)key];
    if (!object)
        return true;

    if (![object isKindOfClass:NSArray.class])
        return false;

    for (id value in object) {
        if (IPC::typeFromObject(value) != targetType)
            return false;
    }

    return true;
}

bool CoreIPCDictionary::collectionValuesAreOfType(const String& key, IPC::NSType keyType, IPC::NSType valueType) const
{
    createNSDictionaryIfNeeded();
    NSDictionary *object = [m_nsDictionary.get() objectForKey:(NSString *)key];
    if (!object)
        return true;

    if (![object isKindOfClass:NSDictionary.class])
        return false;

    for (id key in object) {
        if (IPC::typeFromObject(key) != keyType)
            return false;
        if (IPC::typeFromObject(((NSDictionary *)object)[key]) != valueType)
            return false;
    }

    return true;
}

void CoreIPCDictionary::createNSDictionaryIfNeeded() const
{
    if (!m_nsDictionary) {
        auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_keyValuePairs.size()]);
        for (auto& keyValuePair : m_keyValuePairs)
            [result setObject:keyValuePair.value->toID().get() forKey:keyValuePair.key->toID().get()];
        m_nsDictionary = WTFMove(result);
    }
}

RetainPtr<id> CoreIPCDictionary::toID() const
{
    createNSDictionaryIfNeeded();
    return m_nsDictionary;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
