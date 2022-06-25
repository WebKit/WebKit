/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "MockAppBundleRegistry.h"

#import <wtf/NeverDestroyed.h>

namespace WebPushD {

MockAppBundleRegistry& MockAppBundleRegistry::singleton()
{
    static NeverDestroyed<MockAppBundleRegistry> registry;
    return registry;
}

Vector<String> MockAppBundleRegistry::getOriginsWithRegistrations(const String& hostAppBundleIdentifier)
{
    Vector<String> results;
    auto iterator = m_registeredBundleMap.find(hostAppBundleIdentifier);
    if (iterator != m_registeredBundleMap.end())
        results = copyToVector(iterator->value);
    return results;
}

bool MockAppBundleRegistry::doesBundleExist(const String& hostAppBundleIdentifier, const String& originString)
{
    auto iterator = m_registeredBundleMap.find(hostAppBundleIdentifier);
    return iterator != m_registeredBundleMap.end() && iterator->value.contains(originString);
}

void MockAppBundleRegistry::createBundle(const String& hostAppBundleIdentifier, const String& originString)
{
    m_registeredBundleMap.add(hostAppBundleIdentifier, HashSet<String> { }).iterator->value.add(originString);
}

void MockAppBundleRegistry::deleteBundle(const String& hostAppBundleIdentifier, const String& originString)
{
    auto iterator = m_registeredBundleMap.find(hostAppBundleIdentifier);
    if (iterator == m_registeredBundleMap.end())
        return;

    iterator->value.remove(originString);

    if (iterator->value.isEmpty())
        m_registeredBundleMap.remove(iterator);
}

} // namespace WebPushD
