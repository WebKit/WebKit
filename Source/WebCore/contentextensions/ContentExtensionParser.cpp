/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#include "ContentExtensionParser.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "CSSParser.h"
#include "CSSSelectorList.h"
#include "ContentExtensionError.h"
#include "ContentExtensionRule.h"
#include "ContentExtensionsBackend.h"
#include "ContentExtensionsDebugging.h"
#include <wtf/Expected.h>
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

namespace WebCore::ContentExtensions {
    
static bool containsOnlyASCIIWithNoUppercase(const String& domain)
{
    for (auto character : StringView { domain }.codeUnits()) {
        if (!isASCII(character) || isASCIIUpper(character))
            return false;
    }
    return true;
}
    
static Expected<Vector<String>, std::error_code> getStringList(const JSON::Array& array)
{
    Vector<String> strings;
    strings.reserveInitialCapacity(array.length());
    for (auto& value : array) {
        String string = value->asString();
        if (string.isEmpty())
            return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);
        strings.uncheckedAppend(string);
    }
    return strings;
}

static Expected<Vector<String>, std::error_code> getDomainList(const JSON::Array& arrayObject)
{
    auto domains = getStringList(arrayObject);
    if (!domains)
        return domains;
    
    Vector<String> regexes;
    regexes.reserveInitialCapacity(domains->size());
    for (auto& domain : *domains) {
        // Domains should be punycode encoded lower case.
        if (!containsOnlyASCIIWithNoUppercase(domain))
            return makeUnexpected(ContentExtensionError::JSONDomainNotLowerCaseASCII);

        bool allowSubdomains = false;
        if (domain.startsWith('*')) {
            allowSubdomains = true;
            domain = domain.substring(1);
        }

        std::array<std::pair<UChar, const char*>, 9> escapeTable { {
            { '\\', "\\\\" },
            { '{', "\\{" },
            { '}', "\\}" },
            { '[', "\\[" },
            { '[', "\\[" },
            { '.', "\\." },
            { '?', "\\?" },
            { '*', "\\*" },
            { '$', "\\$" }
        } };
        for (auto& pair : escapeTable)
            domain = domain.replace(pair.first, pair.second);

        const char* protocolRegex = "[a-z][a-z+.-]*:\\/\\/";
        const char* allowSubdomainsRegex = "(.*\\.)*";
        regexes.uncheckedAppend(makeString(protocolRegex, allowSubdomains ? allowSubdomainsRegex : "", domain, "[:/]"));
    }
    return regexes;
}

template<typename T>
static std::error_code getTypeFlags(const JSON::Array& array, ResourceFlags& flags, std::optional<OptionSet<T>> (*stringToType)(StringView))
{
    for (auto& value : array) {
        String name = value->asString();
        auto type = stringToType(StringView(name));
        if (!type)
            return ContentExtensionError::JSONInvalidStringInTriggerFlagsArray;

        flags |= static_cast<ResourceFlags>(type->toRaw());
    }

    return { };
}
    
static Expected<Trigger, std::error_code> loadTrigger(const JSON::Object& ruleObject)
{
    auto triggerObject = ruleObject.getObject("trigger");
    if (!triggerObject)
        return makeUnexpected(ContentExtensionError::JSONInvalidTrigger);

    String urlFilter = triggerObject->getString("url-filter");
    if (urlFilter.isEmpty())
        return makeUnexpected(ContentExtensionError::JSONInvalidURLFilterInTrigger);

    Trigger trigger;
    trigger.urlFilter = urlFilter;

    if (std::optional<bool> urlFilterCaseSensitiveValue = triggerObject->getBoolean("url-filter-is-case-sensitive"))
        trigger.urlFilterIsCaseSensitive = *urlFilterCaseSensitiveValue;

    if (std::optional<bool> topURLFilterCaseSensitiveValue = triggerObject->getBoolean("top-url-filter-is-case-sensitive"))
        trigger.topURLFilterIsCaseSensitive = *topURLFilterCaseSensitiveValue;

    if (std::optional<bool> frameURLFilterCaseSensitiveValue = triggerObject->getBoolean("frame-url-filter-is-case-sensitive"))
        trigger.frameURLFilterIsCaseSensitive = *frameURLFilterCaseSensitiveValue;

    if (auto resourceTypeValue = triggerObject->getValue("resource-type")) {
        auto resourceTypeArray = resourceTypeValue->asArray();
        if (!resourceTypeArray)
            return makeUnexpected(ContentExtensionError::JSONInvalidTriggerFlagsArray);
        if (auto error = getTypeFlags(*resourceTypeArray, trigger.flags, readResourceType))
            return makeUnexpected(error);
    }

    if (auto loadTypeValue = triggerObject->getValue("load-type")) {
        auto loadTypeArray = loadTypeValue->asArray();
        if (!loadTypeArray)
            return makeUnexpected(ContentExtensionError::JSONInvalidTriggerFlagsArray);
        if (auto error = getTypeFlags(*loadTypeArray, trigger.flags, readLoadType))
            return makeUnexpected(error);
    }

    if (auto loadContextValue = triggerObject->getValue("load-context")) {
        auto loadContextArray = loadContextValue->asArray();
        if (!loadContextArray)
            return makeUnexpected(ContentExtensionError::JSONInvalidTriggerFlagsArray);
        if (auto error = getTypeFlags(*loadContextArray, trigger.flags, readLoadContext))
            return makeUnexpected(error);
    }

    auto checkCondition = [&] (ASCIILiteral key, Expected<Vector<String>, std::error_code> (*listReader)(const JSON::Array&), ActionCondition actionCondition) -> std::error_code {
        if (auto value = triggerObject->getValue(key)) {
            if (trigger.flags & ActionConditionMask)
                return ContentExtensionError::JSONMultipleConditions;
            auto array = value->asArray();
            if (!array)
                return ContentExtensionError::JSONInvalidConditionList;
            auto list = listReader(*array);
            if (!list.has_value())
                return list.error();
            trigger.conditions = WTFMove(list.value());
            if (trigger.conditions.isEmpty())
                return ContentExtensionError::JSONInvalidConditionList;
            trigger.flags |= static_cast<ResourceFlags>(actionCondition);
        }
        return { };
    };

    if (auto error = checkCondition("if-domain"_s, getDomainList, ActionCondition::IfTopURL))
        return makeUnexpected(error);

    if (auto error = checkCondition("unless-domain"_s, getDomainList, ActionCondition::UnlessTopURL))
        return makeUnexpected(error);

    if (auto error = checkCondition("if-top-url"_s, getStringList, ActionCondition::IfTopURL))
        return makeUnexpected(error);

    if (auto error = checkCondition("unless-top-url"_s, getStringList, ActionCondition::UnlessTopURL))
        return makeUnexpected(error);

    if (auto error = checkCondition("if-frame-url"_s, getStringList, ActionCondition::IfFrameURL))
        return makeUnexpected(error);

    return trigger;
}

bool isValidCSSSelector(const String& selector)
{
    ASSERT(isMainThread());
    AtomString::init();
    QualifiedName::init();
    CSSParserContext context(HTMLQuirksMode);
    CSSParser parser(context);
    return !!parser.parseSelector(selector);
}

static std::optional<Expected<Action, std::error_code>> loadAction(const JSON::Object& ruleObject)
{
    auto actionObject = ruleObject.getObject("action");
    if (!actionObject)
        return makeUnexpected(ContentExtensionError::JSONInvalidAction);

    String actionType = actionObject->getString("type");

    if (actionType == "block")
        return Action { BlockLoadAction() };
    if (actionType == "ignore-previous-rules")
        return Action { IgnorePreviousRulesAction() };
    if (actionType == "block-cookies")
        return Action { BlockCookiesAction() };
    if (actionType == "css-display-none") {
        String selectorString = actionObject->getString("selector");
        if (!selectorString)
            return makeUnexpected(ContentExtensionError::JSONInvalidCSSDisplayNoneActionType);
        if (!isValidCSSSelector(selectorString))
            return std::nullopt; // Skip rules with invalid selectors to be backwards-compatible.
        return Action { CSSDisplayNoneSelectorAction { { WTFMove(selectorString) } } };
    }
    if (actionType == "make-https")
        return Action { MakeHTTPSAction() };
    if (actionType == "notify") {
        String notification = actionObject->getString("notification");
        if (!notification)
            return makeUnexpected(ContentExtensionError::JSONInvalidNotification);
        return Action { NotifyAction { { WTFMove(notification) } } };
    }
    if (actionType == "redirect") {
        auto action = RedirectAction::parse(*actionObject);
        if (!action)
            return makeUnexpected(action.error());
        return Action { RedirectAction { WTFMove(*action) } };
    }
    if (actionType == "modify-headers") {
        auto action = ModifyHeadersAction::parse(*actionObject);
        if (!action)
            return makeUnexpected(action.error());
        return Action { WTFMove(*action) };
    }
    return makeUnexpected(ContentExtensionError::JSONInvalidActionType);
}

static std::optional<Expected<ContentExtensionRule, std::error_code>> loadRule(const JSON::Object& ruleObject)
{
    auto trigger = loadTrigger(ruleObject);
    if (!trigger.has_value())
        return makeUnexpected(trigger.error());

    auto action = loadAction(ruleObject);
    if (!action)
        return std::nullopt;
    if (!action->has_value())
        return makeUnexpected(action->error());

    return { { { WTFMove(trigger.value()), WTFMove(action->value()) } } };
}

static Expected<Vector<ContentExtensionRule>, std::error_code> loadEncodedRules(const String& ruleJSON)
{
    auto decodedRules = JSON::Value::parseJSON(ruleJSON);

    if (!decodedRules)
        return makeUnexpected(ContentExtensionError::JSONInvalid);

    auto topLevelArray = decodedRules->asArray();
    if (!topLevelArray)
        return makeUnexpected(ContentExtensionError::JSONTopLevelStructureNotAnArray);

    Vector<ContentExtensionRule> ruleList;

    constexpr size_t maxRuleCount = 150000;
    if (topLevelArray->length() > maxRuleCount)
        return makeUnexpected(ContentExtensionError::JSONTooManyRules);
    ruleList.reserveInitialCapacity(topLevelArray->length());
    for (auto& value : *topLevelArray) {
        auto ruleObject = value->asObject();
        if (!ruleObject)
            return makeUnexpected(ContentExtensionError::JSONInvalidRule);

        auto rule = loadRule(*ruleObject);
        if (!rule)
            continue;
        if (!rule->has_value())
            return makeUnexpected(rule->error());
        ruleList.uncheckedAppend(WTFMove(rule->value()));
    }

    return ruleList;
}

Expected<Vector<ContentExtensionRule>, std::error_code> parseRuleList(const String& ruleJSON)
{
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime loadExtensionStartTime = MonotonicTime::now();
#endif

    auto ruleList = loadEncodedRules(ruleJSON);

    if (!ruleList.has_value())
        return makeUnexpected(ruleList.error());

    if (ruleList->isEmpty())
        return makeUnexpected(ContentExtensionError::JSONContainsNoRules);

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime loadExtensionEndTime = MonotonicTime::now();
    dataLogF("Time spent loading extension %f\n", (loadExtensionEndTime - loadExtensionStartTime).seconds());
#endif

    return ruleList;
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
