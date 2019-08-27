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
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/VM.h>
#include <wtf/Expected.h>
#include <wtf/text/WTFString.h>


namespace WebCore {
using namespace JSC;

namespace ContentExtensions {
    
static bool containsOnlyASCIIWithNoUppercase(const String& domain)
{
    for (auto character : StringView { domain }.codeUnits()) {
        if (!isASCII(character) || isASCIIUpper(character))
            return false;
    }
    return true;
}
    
static Expected<Vector<String>, std::error_code> getStringList(ExecState& exec, const JSObject* arrayObject)
{
    static const ContentExtensionError error = ContentExtensionError::JSONInvalidConditionList;
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!arrayObject || !isJSArray(arrayObject))
        return makeUnexpected(error);
    const JSArray* array = jsCast<const JSArray*>(arrayObject);
    
    Vector<String> strings;
    unsigned length = array->length();
    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = array->getIndex(&exec, i);
        if (scope.exception() || !value.isString())
            return makeUnexpected(error);
        
        const String& string = asString(value)->value(&exec);
        if (string.isEmpty())
            return makeUnexpected(error);
        strings.append(string);
    }
    return strings;
}

static Expected<Vector<String>, std::error_code> getDomainList(ExecState& exec, const JSObject* arrayObject)
{
    auto strings = getStringList(exec, arrayObject);
    if (!strings.has_value())
        return strings;
    for (auto& domain : strings.value()) {
        // Domains should be punycode encoded lower case.
        if (!containsOnlyASCIIWithNoUppercase(domain))
            return makeUnexpected(ContentExtensionError::JSONDomainNotLowerCaseASCII);
    }
    return strings;
}

static std::error_code getTypeFlags(ExecState& exec, const JSValue& typeValue, ResourceFlags& flags, uint16_t (*stringToType)(const String&))
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!typeValue.isObject())
        return { };

    const JSObject* object = typeValue.toObject(&exec);
    scope.assertNoException();
    if (!isJSArray(object))
        return ContentExtensionError::JSONInvalidTriggerFlagsArray;

    const JSArray* array = jsCast<const JSArray*>(object);
    
    unsigned length = array->length();
    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = array->getIndex(&exec, i);
        if (scope.exception() || !value)
            return ContentExtensionError::JSONInvalidObjectInTriggerFlagsArray;
        
        String name = value.toWTFString(&exec);
        uint16_t type = stringToType(name);
        if (!type)
            return ContentExtensionError::JSONInvalidStringInTriggerFlagsArray;

        flags |= type;
    }

    return { };
}
    
static Expected<Trigger, std::error_code> loadTrigger(ExecState& exec, const JSObject& ruleObject)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const JSValue triggerObject = ruleObject.get(&exec, Identifier::fromString(vm, "trigger"));
    if (!triggerObject || scope.exception() || !triggerObject.isObject())
        return makeUnexpected(ContentExtensionError::JSONInvalidTrigger);
    
    const JSValue urlFilterObject = triggerObject.get(&exec, Identifier::fromString(vm, "url-filter"));
    if (!urlFilterObject || scope.exception() || !urlFilterObject.isString())
        return makeUnexpected(ContentExtensionError::JSONInvalidURLFilterInTrigger);

    String urlFilter = asString(urlFilterObject)->value(&exec);
    if (urlFilter.isEmpty())
        return makeUnexpected(ContentExtensionError::JSONInvalidURLFilterInTrigger);

    Trigger trigger;
    trigger.urlFilter = urlFilter;

    const JSValue urlFilterCaseValue = triggerObject.get(&exec, Identifier::fromString(vm, "url-filter-is-case-sensitive"));
    if (urlFilterCaseValue && !scope.exception() && urlFilterCaseValue.isBoolean())
        trigger.urlFilterIsCaseSensitive = urlFilterCaseValue.toBoolean(&exec);

    const JSValue topURLFilterCaseValue = triggerObject.get(&exec, Identifier::fromString(vm, "top-url-filter-is-case-sensitive"));
    if (topURLFilterCaseValue && !scope.exception() && topURLFilterCaseValue.isBoolean())
        trigger.topURLConditionIsCaseSensitive = topURLFilterCaseValue.toBoolean(&exec);

    const JSValue resourceTypeValue = triggerObject.get(&exec, Identifier::fromString(vm, "resource-type"));
    if (!scope.exception() && resourceTypeValue.isObject()) {
        auto typeFlagsError = getTypeFlags(exec, resourceTypeValue, trigger.flags, readResourceType);
        if (typeFlagsError)
            return makeUnexpected(typeFlagsError);
    } else if (!resourceTypeValue.isUndefined())
        return makeUnexpected(ContentExtensionError::JSONInvalidTriggerFlagsArray);

    const JSValue loadTypeValue = triggerObject.get(&exec, Identifier::fromString(vm, "load-type"));
    if (!scope.exception() && loadTypeValue.isObject()) {
        auto typeFlagsError = getTypeFlags(exec, loadTypeValue, trigger.flags, readLoadType);
        if (typeFlagsError)
            return makeUnexpected(typeFlagsError);
    } else if (!loadTypeValue.isUndefined())
        return makeUnexpected(ContentExtensionError::JSONInvalidTriggerFlagsArray);

    const JSValue ifDomainValue = triggerObject.get(&exec, Identifier::fromString(vm, "if-domain"));
    if (!scope.exception() && ifDomainValue.isObject()) {
        auto ifDomain = getDomainList(exec, asObject(ifDomainValue));
        if (!ifDomain.has_value())
            return makeUnexpected(ifDomain.error());
        trigger.conditions = WTFMove(ifDomain.value());
        if (trigger.conditions.isEmpty())
            return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);
        ASSERT(trigger.conditionType == Trigger::ConditionType::None);
        trigger.conditionType = Trigger::ConditionType::IfDomain;
    } else if (!ifDomainValue.isUndefined())
        return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);

    const JSValue unlessDomainValue = triggerObject.get(&exec, Identifier::fromString(vm, "unless-domain"));
    if (!scope.exception() && unlessDomainValue.isObject()) {
        if (trigger.conditionType != Trigger::ConditionType::None)
            return makeUnexpected(ContentExtensionError::JSONMultipleConditions);
        auto unlessDomain = getDomainList(exec, asObject(unlessDomainValue));
        if (!unlessDomain.has_value())
            return makeUnexpected(unlessDomain.error());
        trigger.conditions = WTFMove(unlessDomain.value());
        if (trigger.conditions.isEmpty())
            return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);
        trigger.conditionType = Trigger::ConditionType::UnlessDomain;
    } else if (!unlessDomainValue.isUndefined())
        return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);

    const JSValue ifTopURLValue = triggerObject.get(&exec, Identifier::fromString(vm, "if-top-url"));
    if (!scope.exception() && ifTopURLValue.isObject()) {
        if (trigger.conditionType != Trigger::ConditionType::None)
            return makeUnexpected(ContentExtensionError::JSONMultipleConditions);
        auto ifTopURL = getStringList(exec, asObject(ifTopURLValue));
        if (!ifTopURL.has_value())
            return makeUnexpected(ifTopURL.error());
        trigger.conditions = WTFMove(ifTopURL.value());
        if (trigger.conditions.isEmpty())
            return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);
        trigger.conditionType = Trigger::ConditionType::IfTopURL;
    } else if (!ifTopURLValue.isUndefined())
        return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);

    const JSValue unlessTopURLValue = triggerObject.get(&exec, Identifier::fromString(vm, "unless-top-url"));
    if (!scope.exception() && unlessTopURLValue.isObject()) {
        if (trigger.conditionType != Trigger::ConditionType::None)
            return makeUnexpected(ContentExtensionError::JSONMultipleConditions);
        auto unlessTopURL = getStringList(exec, asObject(unlessTopURLValue));
        if (!unlessTopURL.has_value())
            return makeUnexpected(unlessTopURL.error());
        trigger.conditions = WTFMove(unlessTopURL.value());
        if (trigger.conditions.isEmpty())
            return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);
        trigger.conditionType = Trigger::ConditionType::UnlessTopURL;
    } else if (!unlessTopURLValue.isUndefined())
        return makeUnexpected(ContentExtensionError::JSONInvalidConditionList);

    return trigger;
}

bool isValidCSSSelector(const String& selector)
{
    ASSERT(isMainThread());
    AtomString::init();
    QualifiedName::init();
    CSSParserContext context(HTMLQuirksMode);
    CSSParser parser(context);
    CSSSelectorList selectorList;
    parser.parseSelector(selector, selectorList);
    return selectorList.isValid();
}

static Expected<Optional<Action>, std::error_code> loadAction(ExecState& exec, const JSObject& ruleObject)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const JSValue actionObject = ruleObject.get(&exec, Identifier::fromString(vm, "action"));
    if (scope.exception() || !actionObject.isObject())
        return makeUnexpected(ContentExtensionError::JSONInvalidAction);

    const JSValue typeObject = actionObject.get(&exec, Identifier::fromString(vm, "type"));
    if (scope.exception() || !typeObject.isString())
        return makeUnexpected(ContentExtensionError::JSONInvalidActionType);

    String actionType = asString(typeObject)->value(&exec);

    if (actionType == "block")
        return { Action(ActionType::BlockLoad) };
    if (actionType == "ignore-previous-rules")
        return { Action(ActionType::IgnorePreviousRules) };
    if (actionType == "block-cookies")
        return { Action(ActionType::BlockCookies) };
    if (actionType == "css-display-none") {
        JSValue selector = actionObject.get(&exec, Identifier::fromString(vm, "selector"));
        if (scope.exception() || !selector.isString())
            return makeUnexpected(ContentExtensionError::JSONInvalidCSSDisplayNoneActionType);

        String selectorString = asString(selector)->value(&exec);
        if (!isValidCSSSelector(selectorString)) {
            // Skip rules with invalid selectors to be backwards-compatible.
            return { WTF::nullopt };
        }
        return { Action(ActionType::CSSDisplayNoneSelector, selectorString) };
    }
    if (actionType == "make-https")
        return { Action(ActionType::MakeHTTPS) };
    if (actionType == "notify") {
        JSValue notification = actionObject.get(&exec, Identifier::fromString(vm, "notification"));
        if (scope.exception() || !notification.isString())
            return makeUnexpected(ContentExtensionError::JSONInvalidNotification);
        return { Action(ActionType::Notify, asString(notification)->value(&exec)) };
    }
    return makeUnexpected(ContentExtensionError::JSONInvalidActionType);
}

static Expected<Optional<ContentExtensionRule>, std::error_code> loadRule(ExecState& exec, const JSObject& ruleObject)
{
    auto trigger = loadTrigger(exec, ruleObject);
    if (!trigger.has_value())
        return makeUnexpected(trigger.error());

    auto action = loadAction(exec, ruleObject);
    if (!action.has_value())
        return makeUnexpected(action.error());

    if (action.value())
        return {{{ WTFMove(trigger.value()), WTFMove(action.value().value()) }}};

    return { WTF::nullopt };
}

static Expected<Vector<ContentExtensionRule>, std::error_code> loadEncodedRules(ExecState& exec, const String& ruleJSON)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: JSONParse should require callbacks instead of an ExecState.
    const JSValue decodedRules = JSONParse(&exec, ruleJSON);

    if (scope.exception() || !decodedRules)
        return makeUnexpected(ContentExtensionError::JSONInvalid);

    if (!decodedRules.isObject())
        return makeUnexpected(ContentExtensionError::JSONTopLevelStructureNotAnObject);

    const JSObject* topLevelObject = decodedRules.toObject(&exec);
    if (!topLevelObject || scope.exception())
        return makeUnexpected(ContentExtensionError::JSONTopLevelStructureNotAnObject);
    
    if (!isJSArray(topLevelObject))
        return makeUnexpected(ContentExtensionError::JSONTopLevelStructureNotAnArray);

    const JSArray* topLevelArray = jsCast<const JSArray*>(topLevelObject);

    Vector<ContentExtensionRule> ruleList;

    unsigned length = topLevelArray->length();
    const unsigned maxRuleCount = 50000;
    if (length > maxRuleCount)
        return makeUnexpected(ContentExtensionError::JSONTooManyRules);
    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = topLevelArray->getIndex(&exec, i);
        if (scope.exception() || !value)
            return makeUnexpected(ContentExtensionError::JSONInvalidObjectInTopLevelArray);

        const JSObject* ruleObject = value.toObject(&exec);
        if (!ruleObject || scope.exception())
            return makeUnexpected(ContentExtensionError::JSONInvalidRule);

        auto rule = loadRule(exec, *ruleObject);
        if (!rule.has_value())
            return makeUnexpected(rule.error());
        if (rule.value())
            ruleList.append(WTFMove(*rule.value()));
    }

    return ruleList;
}

Expected<Vector<ContentExtensionRule>, std::error_code> parseRuleList(const String& ruleJSON)
{
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime loadExtensionStartTime = MonotonicTime::now();
#endif
    RefPtr<VM> vm = VM::create();

    JSLockHolder locker(vm.get());
    JSGlobalObject* globalObject = JSGlobalObject::create(*vm, JSGlobalObject::createStructure(*vm, jsNull()));

    ExecState* exec = globalObject->globalExec();
    auto ruleList = loadEncodedRules(*exec, ruleJSON);

    vm = nullptr;

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

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
