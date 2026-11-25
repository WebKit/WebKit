/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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

#pragma once

#include <WebCore/PlatformLocale.h>
#include <wtf/Forward.h>
#include <wtf/JSONValues.h>
#include <wtf/Noncopyable.h>

namespace JSC { namespace Yarr {
class RegularExpression;
} }

namespace WebKit {
class WebExtension;

class WebExtensionLocalization : public RefCounted<WebExtensionLocalization> {
    WTF_MAKE_NONCOPYABLE(WebExtensionLocalization)

public:
    template<typename... Args>
    static Ref<WebExtensionLocalization> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionLocalization(std::forward<Args>(args)...));
    }

    const RefPtr<StringImpl>& uniqueIdentifier() LIFETIME_BOUND { return m_uniqueIdentifier; };
    RefPtr<JSON::Object> localizationJSON() { return m_localizationJSON; };

    RefPtr<JSON::Object> localizedJSONforJSON(RefPtr<JSON::Object>);
    RefPtr<StringImpl> localizedStringForKey(RefPtr<StringImpl> key, Vector<RefPtr<StringImpl>> placeholders = { });
    RefPtr<StringImpl> localizedStringForString(RefPtr<StringImpl>);

private:
    explicit WebExtensionLocalization(WebKit::WebExtension&);
    explicit WebExtensionLocalization(RefPtr<JSON::Object> localizedJSON, StringImpl* uniqueIdentifier);
    explicit WebExtensionLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, StringImpl* withBestLocale, StringImpl* uniqueIdentifier);

    void loadRegionalLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, StringImpl* withBestLocale = nullptr, StringImpl* uniqueIdentifier = nullptr);

    RefPtr<JSON::Object> localizationJSONForWebExtension(WebKit::WebExtension&, StringImpl* withLocale);
    RefPtr<JSON::Array> localizedArrayForArray(RefPtr<JSON::Array>);
    Ref<JSON::Object> predefinedMessages();

    RefPtr<StringImpl> stringByReplacingNamedPlaceholdersInString(RefPtr<StringImpl> sourceString, RefPtr<JSON::Object> placeholders);
    RefPtr<StringImpl> stringByReplacingPositionalPlaceholdersInString(RefPtr<StringImpl> sourceString, Vector<RefPtr<StringImpl>> placeholders = { });

    std::unique_ptr<WebCore::Locale> m_locale;
    RefPtr<StringImpl> m_localeString;
    RefPtr<StringImpl> m_uniqueIdentifier;
    RefPtr<JSON::Object> m_localizationJSON;
};

}
