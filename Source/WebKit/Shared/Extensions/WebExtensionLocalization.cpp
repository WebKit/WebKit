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

#include "config.h"
#include "WebExtensionLocalization.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Logging.h"
#include "WebExtension.h"
#include "WebExtensionUtilities.h"
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/Language.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static constexpr auto messageKey = "message"_s;
static constexpr auto placeholdersKey = "placeholders"_s;
static constexpr auto placeholderDictionaryContentKey = "content"_s;

static constexpr auto predefinedMessageUILocale = "@@ui_locale"_s;
static constexpr auto predefinedMessageLanguageDirection = "@@bidi_dir"_s;
static constexpr auto predefinedMessageLanguageDirectionReversed = "@@bidi_reversed_dir"_s;
static constexpr auto predefinedMessageTextLeadingEdge = "@@bidi_start_edge"_s;
static constexpr auto predefinedMessageTextTrailingEdge = "@@bidi_end_edge"_s;
static constexpr auto predefinedMessageExtensionID = "@@extension_id"_s;

static constexpr auto predefinedMessageValueLeftToRight = "ltr"_s;
static constexpr auto predefinedMessageValueRightToLeft = "rtl"_s;
static constexpr auto predefinedMessageValueTextEdgeLeft = "left"_s;
static constexpr auto predefinedMessageValueTextEdgeRight = "right"_s;

WebExtensionLocalization::WebExtensionLocalization(WebExtension& webExtension)
{
    auto defaultLocaleString = webExtension.defaultLocale();
    if (defaultLocaleString.isEmpty()) {
        RELEASE_LOG_DEBUG(Extensions, "No default locale provided");
        loadRegionalLocalization(nullptr, nullptr, nullptr, emptyString(), emptyString());
        return;
    }

    RefPtr defaultLocaleJSON = localizationJSONForWebExtension(webExtension, defaultLocaleString);
    if (!defaultLocaleJSON) {
        RELEASE_LOG_DEBUG(Extensions, "No localization found for default locale %s", defaultLocaleString.utf8().data());
        loadRegionalLocalization(nullptr, nullptr, nullptr, emptyString(), emptyString());
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Loaded default locale %s", defaultLocaleString.utf8().data());

    auto bestLocaleString = webExtension.bestMatchLocale();
    auto defaultLocaleComponents = parseLocale(defaultLocaleString);
    auto bestLocaleComponents = parseLocale(bestLocaleString);
    auto bestLocaleLanguageOnlyString = bestLocaleComponents.languageCode;

    RELEASE_LOG_DEBUG(Extensions, "Best locale is %s", bestLocaleString.utf8().data());

    RefPtr<JSON::Object> languageJSON;
    if (!bestLocaleLanguageOnlyString.isEmpty() && bestLocaleLanguageOnlyString != defaultLocaleString) {
        languageJSON = localizationJSONForWebExtension(webExtension, bestLocaleLanguageOnlyString);
        if (languageJSON)
            RELEASE_LOG_DEBUG(Extensions, "Loaded language-only locale %s", bestLocaleLanguageOnlyString.utf8().data());
    }

    RefPtr<JSON::Object> regionalJSON;
    if (bestLocaleString != bestLocaleLanguageOnlyString && bestLocaleString != defaultLocaleString) {
        regionalJSON = localizationJSONForWebExtension(webExtension, bestLocaleString);
        if (regionalJSON)
            RELEASE_LOG_DEBUG(Extensions, "Loaded regional locale %s", bestLocaleString.utf8().data());
    }

    loadRegionalLocalization(regionalJSON, languageJSON, defaultLocaleJSON, bestLocaleString);
}

WebExtensionLocalization::WebExtensionLocalization(RefPtr<JSON::Object> localizedJSON, const String& uniqueIdentifier)
{
    ASSERT(!uniqueIdentifier.isEmpty());
    ASSERT(localizedJSON);

    auto localeString = emptyString();
    if (RefPtr predefinedMessages = localizedJSON->getObject(predefinedMessageUILocale))
        localeString = predefinedMessages->getString(messageKey);

    loadRegionalLocalization(localizedJSON, nullptr, nullptr, localeString, uniqueIdentifier);
}

WebExtensionLocalization::WebExtensionLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, const String& bestLocale, const String& uniqueIdentifier)
{
    loadRegionalLocalization(regionalLocalization, languageLocalization, defaultLocalization, bestLocale, uniqueIdentifier);
}

void WebExtensionLocalization::loadRegionalLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, const String& bestLocale, const String& uniqueIdentifier)
{
    m_locale = WebCore::Locale::create(AtomString { bestLocale });
    m_localeString = bestLocale;
    m_uniqueIdentifier = uniqueIdentifier;

    RefPtr localizationJSON = predefinedMessages();
    localizationJSON = mergeJSON(localizationJSON, jsonWithLowercaseKeys(regionalLocalization));
    localizationJSON = mergeJSON(localizationJSON, jsonWithLowercaseKeys(languageLocalization));
    localizationJSON = mergeJSON(localizationJSON, jsonWithLowercaseKeys(defaultLocalization));

    ASSERT(localizationJSON);

    m_localizationJSON = localizationJSON;

    return;
}

RefPtr<JSON::Object> WebExtensionLocalization::localizedJSONforJSON(RefPtr<JSON::Object> json)
{
    if (!json)
        return nullptr;

    Ref newObject = JSON::Object::create();

    for (auto& key : json->keys()) {
        auto valueType = json->getValue(key)->type();

        if (valueType == JSON::Value::Type::String)
            newObject->setString(key, localizedStringForString(json->getString(key)));
        else if (valueType == JSON::Value::Type::Array)
            newObject->setArray(key, *localizedArrayForArray(json->getArray(key)));
        else if (valueType == JSON::Value::Type::Object)
            newObject->setObject(key, *localizedJSONforJSON(json->getObject(key)));
        else
            newObject->setValue(key, *json->getValue(key));
    }

    return newObject;
}

String WebExtensionLocalization::localizedStringForKey(String key, Vector<String> placeholders)
{
    if (placeholders.size() > 9)
        return emptyString();

    if (!m_localizationJSON || !m_localizationJSON->size())
        return emptyString();

    if (key.contains("__MSG_"_s)) {
        key = key.substring(6, key.length());
        key = key.substring(0, key.length() - 2);
    }

    RefPtr localizationJSON = m_localizationJSON;
    if (!localizationJSON)
        return emptyString();

    RefPtr stringJSON = localizationJSON->getObject(key.convertToASCIILowercase());
    if (!stringJSON)
        return emptyString();

    auto localizedString = stringJSON->getString(messageKey);
    if (!localizedString.length())
        return emptyString();

    RefPtr namedPlaceholders = jsonWithLowercaseKeys(stringJSON->getObject(placeholdersKey));

    localizedString = stringByReplacingNamedPlaceholdersInString(localizedString, namedPlaceholders);
    localizedString = stringByReplacingPositionalPlaceholdersInString(localizedString, placeholders);

    localizedString = localizedString.impl()->replace("$$"_s, "$"_s);

    return localizedString;
}

RefPtr<JSON::Array> WebExtensionLocalization::localizedArrayForArray(RefPtr<JSON::Array> json)
{
    if (!json)
        return nullptr;

    Ref newArray = JSON::Array::create();

    for (Ref value : *json) {
        auto valueType = value->type();

        if (valueType == JSON::Value::Type::String)
            newArray->pushString(localizedStringForString(value->asString()));
        else if (valueType == JSON::Value::Type::Array)
            newArray->pushArray(*localizedArrayForArray(value->asArray()));
        else if (valueType == JSON::Value::Type::Object)
            newArray->pushObject(*localizedJSONforJSON(value->asObject()));
        else
            newArray->pushValue(WTFMove(value));
    }

    return newArray;
}

String WebExtensionLocalization::localizedStringForString(String sourceString)
{
    String localizedString = sourceString;

    auto localizableStringRegularExpression = JSC::Yarr::RegularExpression("__MSG_([A-Za-z0-9_@]+)__"_s, { });

    int index = 0;
    while (index < static_cast<int>(localizedString.length())) {
        int matchLength;
        index = localizableStringRegularExpression.match(localizedString, index, &matchLength);
        if (index < 0)
            break;

        auto key = localizedString.substring(index, matchLength);
        auto localizedReplacement = localizedStringForKey(key);

        localizedString = makeStringByReplacingAll(localizedString, key, localizedReplacement);

        index += localizedReplacement.length();
    }

    return localizedString;
}

RefPtr<JSON::Object> WebExtensionLocalization::localizationJSONForWebExtension(WebExtension& webExtension, const String& locale)
{
    StringBuilder pathBuilder;
    pathBuilder.append("_locales/"_s);
    pathBuilder.append(locale);
    pathBuilder.append("/messages.json"_s);

    auto path = pathBuilder.toString();

    RefPtr<API::Error> error;
    auto jsonString = webExtension.resourceStringForPath(path, error, WebExtension::CacheResult::No, WebExtension::SuppressNotFoundErrors::Yes);
    if (error) {
        webExtension.recordErrorIfNeeded(error);
        return nullptr;
    }

    RefPtr json = JSON::Value::parseJSON(jsonString);
    return json ? json->asObject() : nullptr;
}

Ref<JSON::Object> WebExtensionLocalization::predefinedMessages()
{
    Ref predefinedMessages = JSON::Object::create();

    auto createMessageKey = [](String messageValue) {
        Ref object = JSON::Object::create();
        object->setString(messageKey, messageValue);
        return object;
    };

    if (m_locale && !m_localeString.isEmpty()) {
        if (m_locale->defaultWritingDirection() == WebCore::Locale::WritingDirection::LeftToRight) {
            predefinedMessages->setObject(predefinedMessageLanguageDirection, createMessageKey(predefinedMessageValueLeftToRight));
            predefinedMessages->setObject(predefinedMessageLanguageDirectionReversed, createMessageKey(predefinedMessageValueRightToLeft));
            predefinedMessages->setObject(predefinedMessageTextLeadingEdge, createMessageKey(predefinedMessageValueTextEdgeLeft));
            predefinedMessages->setObject(predefinedMessageTextTrailingEdge, createMessageKey(predefinedMessageValueTextEdgeRight));
        } else {
            predefinedMessages->setObject(predefinedMessageLanguageDirection, createMessageKey(predefinedMessageValueRightToLeft));
            predefinedMessages->setObject(predefinedMessageLanguageDirectionReversed, createMessageKey(predefinedMessageValueLeftToRight));
            predefinedMessages->setObject(predefinedMessageTextLeadingEdge, createMessageKey(predefinedMessageValueTextEdgeRight));
            predefinedMessages->setObject(predefinedMessageTextTrailingEdge, createMessageKey(predefinedMessageValueTextEdgeLeft));
        }
    }

    if (!m_localeString.isNull())
        predefinedMessages->setObject(predefinedMessageUILocale, createMessageKey(m_localeString));

    if (!m_uniqueIdentifier.isNull())
        predefinedMessages->setObject(predefinedMessageExtensionID, createMessageKey(m_uniqueIdentifier));

    return predefinedMessages;
}

String WebExtensionLocalization::stringByReplacingNamedPlaceholdersInString(String sourceString, RefPtr<JSON::Object> placeholders)
{
    String localizedString = sourceString;

    auto localizableStringRegularExpression = JSC::Yarr::RegularExpression("(?:[^$]|^)(\\$([A-Za-z0-9_@]+)\\$)"_s, { });

    int index = 0;
    while (index < static_cast<int>(localizedString.length())) {
        int matchLength;
        index = localizableStringRegularExpression.match(localizedString, index, &matchLength);
        if (index < 0)
            break;

        auto originalKey = localizedString.substring(index, matchLength);

        if (originalKey.startsWith(' '))
            originalKey = localizedString.substring(index + 1, matchLength - 1);

        auto key = originalKey.trim(isASCIIWhitespace).substring(1, originalKey.length() - 2).convertToASCIILowercase();

        auto localizedReplacement = placeholders->getObject(key) ? placeholders->getObject(key)->getString(placeholderDictionaryContentKey) : emptyString();

        localizedString = makeStringByReplacingAll(localizedString, originalKey, localizedReplacement);

        index += localizedReplacement.length();
    }

    return localizedString;
}

String WebExtensionLocalization::stringByReplacingPositionalPlaceholdersInString(String sourceString, Vector<String> placeholders)
{
    auto localizedString = sourceString;

    auto localizableStringRegularExpression = JSC::Yarr::RegularExpression("(?:[^$]|^)(\\$([0-9]))"_s, { });

    int index = 0;
    while (index < static_cast<int>(localizedString.length())) {
        int matchLength;
        index = localizableStringRegularExpression.match(localizedString, index, &matchLength);
        if (index < 0)
            break;

        auto originalKey = localizedString.substring(index, matchLength).trim(isASCIIWhitespace<UChar>);
        auto key = originalKey.substring(1, originalKey.length());
        auto keyInteger = parseInteger<size_t>(key);

        String replacement;
        if (keyInteger && placeholders.size() && *keyInteger <= placeholders.size() && keyInteger > 0)
            replacement = placeholders[*keyInteger - 1];
        else
            replacement = emptyString();

        localizedString = makeStringByReplacingAll(localizedString, originalKey, replacement);

        if (!matchLength)
            break;

        index += replacement.length() + 2;
    }

    return localizedString;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
