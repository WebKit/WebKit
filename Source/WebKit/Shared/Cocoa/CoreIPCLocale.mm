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
#import "CoreIPCLocale.h"

#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringHash.h>

namespace WebKit {

bool CoreIPCLocale::isValidIdentifier(const String& identifier)
{
    if ([[NSLocale availableLocaleIdentifiers] containsObject:identifier])
        return true;
    if (canonicalLocaleStringReplacement(identifier))
        return true;
    return false;
}

CoreIPCLocale::CoreIPCLocale(NSLocale *locale)
    : m_identifier([locale localeIdentifier])
{
}

CoreIPCLocale::CoreIPCLocale(String&& identifier)
    : m_identifier([[NSLocale currentLocale] localeIdentifier])
{
    if ([[NSLocale availableLocaleIdentifiers] containsObject:identifier])
        m_identifier = identifier;
    else if (auto fixedLocale = canonicalLocaleStringReplacement(identifier))
        m_identifier = *fixedLocale;
}

RetainPtr<id> CoreIPCLocale::toID() const
{
    return [[NSLocale alloc] initWithLocaleIdentifier:(NSString *)m_identifier];
}

std::optional<String> CoreIPCLocale::canonicalLocaleStringReplacement(const String& identifier)
{
    static NeverDestroyed<RetainPtr<NSDictionary>> dictionary = [] {
        RetainPtr dictionary = adoptNS([NSMutableDictionary new]);
        for (NSString *input in [NSLocale availableLocaleIdentifiers]) {
            NSString *output = [[NSLocale localeWithLocaleIdentifier:input] localeIdentifier];
            if (![output isEqualToString:input])
                [dictionary setObject:input forKey:output];
        }
        return dictionary;
    }();
    if (NSString *entry = [dictionary.get() objectForKey:identifier])
        return entry;
    return std::nullopt;
}

}
