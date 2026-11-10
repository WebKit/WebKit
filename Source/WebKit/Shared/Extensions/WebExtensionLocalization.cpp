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
#include <wtf/Borrow.h>
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
    if (isEmpty(defaultLocaleString)) {
        RELEASE_LOG_DEBUG(Extensions, "No default locale provided");
        loadRegionalLocalization(nullptr, nullptr, nullptr, emptyString(), emptyString());
        return;
    }

    RefPtr defaultLocaleJSON = localizationJSONForWebExtension(webExtension, defaultLocaleString);
    if (!defaultLocaleJSON) {
        RELEASE_LOG_DEBUG(Extensions, "No localization found for default locale %s", utf8(defaultLocaleString));
        loadRegionalLocalization(nullptr, nullptr, nullptr, emptyString(), emptyString());
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Loaded default locale %s", utf8(defaultLocaleString));

    auto bestLocaleString = webExtension.bestMatchLocale();
    auto defaultLocaleComponents = parseLocale(defaultLocaleString);
    auto bestLocaleComponents = parseLocale(bestLocaleString);
    auto bestLocaleLanguageOnlyString = bestLocaleComponents.languageCode;

    RELEASE_LOG_DEBUG(Extensions, "Best locale is %s", utf8(bestLocaleString));

    RefPtr<JSON::Object> languageJSON;
    if (!isEmpty(bestLocaleLanguageOnlyString) && !equal(bestLocaleLanguageOnlyString, defaultLocaleString)) {
        languageJSON = localizationJSONForWebExtension(webExtension, bestLocaleLanguageOnlyString);
        if (languageJSON)
            RELEASE_LOG_DEBUG(Extensions, "Loaded language-only locale %s", utf8(bestLocaleLanguageOnlyString));
    }

    RefPtr<JSON::Object> regionalJSON;
    if (!equal(bestLocaleString, bestLocaleLanguageOnlyString) && !equal(bestLocaleString, defaultLocaleString)) {
        regionalJSON = localizationJSONForWebExtension(webExtension, bestLocaleString);
        if (regionalJSON)
            RELEASE_LOG_DEBUG(Extensions, "Loaded regional locale %s", utf8(bestLocaleString));
    }

    loadRegionalLocalization(regionalJSON, languageJSON, defaultLocaleJSON, bestLocaleString);
}

WebExtensionLocalization::WebExtensionLocalization(RefPtr<JSON::Object> localizedJSON, StringImpl* uniqueIdentifier)
{
    ASSERT(!isEmpty(uniqueIdentifier));
    ASSERT(localizedJSON);

    auto localeString = emptyString();
    if (RefPtr predefinedMessages = localizedJSON->getObject(StringImpl::create(predefinedMessageUILocale).ptr()))
        localeString = predefinedMessages->getString(StringImpl::create(messageKey).ptr());

    loadRegionalLocalization(localizedJSON, nullptr, nullptr, localeString, uniqueIdentifier);
}

WebExtensionLocalization::WebExtensionLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, StringImpl* bestLocale, StringImpl* uniqueIdentifier)
{
    loadRegionalLocalization(regionalLocalization, languageLocalization, defaultLocalization, bestLocale, uniqueIdentifier);
}

void WebExtensionLocalization::loadRegionalLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, StringImpl* bestLocale, StringImpl* uniqueIdentifier)
{
    m_locale = WebCore::Locale::create(AtomStringImpl::add(bestLocale).get());
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

    for (auto& key : borrow(json->keys())) {
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

RefPtr<StringImpl> WebExtensionLocalization::localizedStringForKey(RefPtr<StringImpl> key, Vector<RefPtr<StringImpl>> placeholders)
{
    if (placeholders.size() > 9)
        return emptyString();

    if (!m_localizationJSON || !m_localizationJSON->size())
        return emptyString();

    if (contains(key, "__MSG_"_s)) {
        key = substring(key, 6, length(key));
        key = substring(key, 0, length(key) - 2);
    }

    RefPtr localizationJSON = m_localizationJSON;
    if (!localizationJSON)
        return emptyString();

    RefPtr stringJSON = localizationJSON->getObject(convertToASCIILowercase(key));
    if (!stringJSON)
        return emptyString();

    auto localizedString = stringJSON->getString(StringImpl::create(messageKey).ptr());
    if (!length(localizedString))
        return emptyString();

    RefPtr namedPlaceholders = jsonWithLowercaseKeys(stringJSON->getObject(StringImpl::create(placeholdersKey).ptr()));

    localizedString = stringByReplacingNamedPlaceholdersInString(localizedString, namedPlaceholders);
    localizedString = stringByReplacingPositionalPlaceholdersInString(localizedString, placeholders);

    localizedString = localizedString.get()->replace("$$"_s, "$"_s);

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
            newArray->pushValue(WTF::move(value));
    }

    return newArray;
}

RefPtr<StringImpl> WebExtensionLocalization::localizedStringForString(RefPtr<StringImpl> sourceString)
{
    RefPtr<StringImpl> localizedString = sourceString;

    auto localizableStringRegularExpression = JSC::Yarr::RegularExpression("__MSG_([A-Za-z0-9_@]+)__"_s, { });

    int index = 0;
    while (index < static_cast<int>(length(localizedString))) {
        int matchLength;
        index = localizableStringRegularExpression.match(localizedString, index, &matchLength);
        if (index < 0)
            break;

        auto key = substring(localizedString, index, matchLength);
        auto localizedReplacement = localizedStringForKey(key);

        localizedString = makeStringByReplacingAll(localizedString, key, localizedReplacement);

        index += length(localizedReplacement);
    }

    return localizedString;
}

RefPtr<JSON::Object> WebExtensionLocalization::localizationJSONForWebExtension(WebExtension& webExtension, StringImpl* locale)
{
    StringBuilder pathBuilder;
    pathBuilder.append("_locales/"_s);
    pathBuilder.append(locale);
    pathBuilder.append("/messages.json"_s);

    auto path = pathBuilder.toString();

    auto jsonStringResult = webExtension.resourceStringForPath(path, WebExtension::CacheResult::No, WebExtension::SuppressNotFoundErrors::Yes);
    if (!jsonStringResult) {
        webExtension.recordErrorIfNeeded(jsonStringResult.error());
        return nullptr;
    }

    RefPtr json = JSON::Value::parseJSON(jsonStringResult.value());
    return json ? json->asObject() : nullptr;
}

Ref<JSON::Object> WebExtensionLocalization::predefinedMessages()
{
    Ref predefinedMessages = JSON::Object::create();

    auto createMessageKey = [](RefPtr<StringImpl> messageValue) {
        Ref object = JSON::Object::create();
        object->setString(StringImpl::create(messageKey).ptr(), messageValue);
        return object;
    };

    if (CheckedPtr locale = m_locale.get(); locale && !isEmpty(m_localeString)) {
        if (locale->defaultWritingDirection() == WebCore::Locale::WritingDirection::LeftToRight) {
            predefinedMessages->setObject(StringImpl::create(predefinedMessageLanguageDirection).ptr(), createMessageKey(predefinedMessageValueLeftToRight));
            predefinedMessages->setObject(StringImpl::create(predefinedMessageLanguageDirectionReversed).ptr(), createMessageKey(predefinedMessageValueRightToLeft));
            predefinedMessages->setObject(StringImpl::create(predefinedMessageTextLeadingEdge).ptr(), createMessageKey(predefinedMessageValueTextEdgeLeft));
            predefinedMessages->setObject(StringImpl::create(predefinedMessageTextTrailingEdge).ptr(), createMessageKey(predefinedMessageValueTextEdgeRight));
        } else {
            predefinedMessages->setObject(StringImpl::create(predefinedMessageLanguageDirection).ptr(), createMessageKey(predefinedMessageValueRightToLeft));
            predefinedMessages->setObject(StringImpl::create(predefinedMessageLanguageDirectionReversed).ptr(), createMessageKey(predefinedMessageValueLeftToRight));
            predefinedMessages->setObject(StringImpl::create(predefinedMessageTextLeadingEdge).ptr(), createMessageKey(predefinedMessageValueTextEdgeRight));
            predefinedMessages->setObject(StringImpl::create(predefinedMessageTextTrailingEdge).ptr(), createMessageKey(predefinedMessageValueTextEdgeLeft));
        }
    }

    if (m_localeString)
        predefinedMessages->setObject(StringImpl::create(predefinedMessageUILocale).ptr(), createMessageKey(m_localeString));

    if (m_uniqueIdentifier)
        predefinedMessages->setObject(StringImpl::create(predefinedMessageExtensionID).ptr(), createMessageKey(m_uniqueIdentifier));

    return predefinedMessages;
}

RefPtr<StringImpl> WebExtensionLocalization::stringByReplacingNamedPlaceholdersInString(RefPtr<StringImpl> sourceString, RefPtr<JSON::Object> placeholders)
{
    RefPtr<StringImpl> localizedString = sourceString;

    auto localizableStringRegularExpression = JSC::Yarr::RegularExpression("(?:[^$]|^)(\\$([A-Za-z0-9_@]+)\\$)"_s, { });

    int index = 0;
    while (index < static_cast<int>(length(localizedString))) {
        int matchLength;
        index = localizableStringRegularExpression.match(localizedString, index, &matchLength);
        if (index < 0)
            break;

        auto originalKey = substring(localizedString, index, matchLength);

        if (!startsWith(originalKey, '$'))
            originalKey = substring(localizedString, index + 1, matchLength - 1);

        auto key = convertToASCIILowercase(substring(originalKey, 1, length(originalKey) - 2));

        auto localizedReplacement = (placeholders && placeholders->getObject(key)) ? placeholders->getObject(key)->getString(StringImpl::create(placeholderDictionaryContentKey).ptr()) : emptyString();

        localizedString = makeStringByReplacingAll(localizedString, originalKey, localizedReplacement);

        index += std::max(0, static_cast<int>(length(localizedReplacement)) - 1);
    }

    return localizedString;
}

RefPtr<StringImpl> WebExtensionLocalization::stringByReplacingPositionalPlaceholdersInString(RefPtr<StringImpl> sourceString, Vector<RefPtr<StringImpl>> placeholders)
{
    auto localizedString = sourceString;

    auto localizableStringRegularExpression = JSC::Yarr::RegularExpression("(?:[^$]|^)(\\$([0-9]))"_s, { });

    int index = 0;
    while (index < static_cast<int>(length(localizedString))) {
        int matchLength;
        index = localizableStringRegularExpression.match(localizedString, index, &matchLength);
        if (index < 0)
            break;

        auto originalKey = substring(localizedString, index, matchLength);

        if (!startsWith(originalKey, '$'))
            originalKey = substring(localizedString, index + 1, matchLength - 1);

        auto keyInteger = parseInteger<size_t>(substring(originalKey, 1, length(originalKey) - 1));

        RefPtr<StringImpl> replacement;
        if (keyInteger && placeholders.size() && *keyInteger <= placeholders.size() && keyInteger > 0)
            replacement = borrow(placeholders)[*keyInteger - 1];
        else
            replacement = emptyString();

        localizedString = makeStringByReplacingAll(localizedString, originalKey, replacement);

        if (!matchLength)
            break;

        index += std::max(0, static_cast<int>(length(replacement)) - 1);
    }

    return localizedString;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
