/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "CSSParserMode.h"
#include "CSSSelectorList.h"
#include "ContentExtensionError.h"
#include "ContentExtensionRule.h"
#include "ContentExtensionsBackend.h"
#include "ContentExtensionsDebugging.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/VM.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

namespace ContentExtensions {
    
static bool containsOnlyASCIIWithNoUppercase(const String& domain)
{
    for (unsigned i = 0; i < domain.length(); ++i) {
        UChar c = domain.at(i);
        if (!isASCII(c) || isASCIIUpper(c))
            return false;
    }
    return true;
}
    
static std::error_code getDomainList(ExecState& exec, const JSObject* arrayObject, Vector<String>& vector)
{
    ASSERT(vector.isEmpty());
    if (!arrayObject || !isJSArray(arrayObject))
        return ContentExtensionError::JSONInvalidDomainList;
    const JSArray* array = jsCast<const JSArray*>(arrayObject);
    
    unsigned length = array->length();
    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = array->getIndex(&exec, i);
        if (exec.hadException() || !value.isString())
            return ContentExtensionError::JSONInvalidDomainList;
        
        // Domains should be punycode encoded lower case.
        const String& domain = jsCast<JSString*>(value)->value(&exec);
        if (domain.isEmpty())
            return ContentExtensionError::JSONInvalidDomainList;
        if (!containsOnlyASCIIWithNoUppercase(domain))
            return ContentExtensionError::JSONDomainNotLowerCaseASCII;
        vector.append(domain);
    }
    return { };
}

static std::error_code getTypeFlags(ExecState& exec, const JSValue& typeValue, ResourceFlags& flags, uint16_t (*stringToType)(const String&))
{
    if (!typeValue.isObject())
        return { };

    const JSObject* object = typeValue.toObject(&exec);
    ASSERT(!exec.hadException());
    if (!isJSArray(object))
        return ContentExtensionError::JSONInvalidTriggerFlagsArray;

    const JSArray* array = jsCast<const JSArray*>(object);
    
    unsigned length = array->length();
    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = array->getIndex(&exec, i);
        if (exec.hadException() || !value)
            return ContentExtensionError::JSONInvalidObjectInTriggerFlagsArray;
        
        String name = value.toWTFString(&exec);
        uint16_t type = stringToType(name);
        if (!type)
            return ContentExtensionError::JSONInvalidStringInTriggerFlagsArray;

        flags |= type;
    }

    return { };
}
    
static std::error_code loadTrigger(ExecState& exec, const JSObject& ruleObject, Trigger& trigger)
{
    const JSValue triggerObject = ruleObject.get(&exec, Identifier::fromString(&exec, "trigger"));
    if (!triggerObject || exec.hadException() || !triggerObject.isObject())
        return ContentExtensionError::JSONInvalidTrigger;
    
    const JSValue urlFilterObject = triggerObject.get(&exec, Identifier::fromString(&exec, "url-filter"));
    if (!urlFilterObject || exec.hadException() || !urlFilterObject.isString())
        return ContentExtensionError::JSONInvalidURLFilterInTrigger;

    String urlFilter = urlFilterObject.toWTFString(&exec);
    if (urlFilter.isEmpty())
        return ContentExtensionError::JSONInvalidURLFilterInTrigger;

    trigger.urlFilter = urlFilter;

    const JSValue urlFilterCaseValue = triggerObject.get(&exec, Identifier::fromString(&exec, "url-filter-is-case-sensitive"));
    if (urlFilterCaseValue && !exec.hadException() && urlFilterCaseValue.isBoolean())
        trigger.urlFilterIsCaseSensitive = urlFilterCaseValue.toBoolean(&exec);

    const JSValue resourceTypeValue = triggerObject.get(&exec, Identifier::fromString(&exec, "resource-type"));
    if (!exec.hadException() && resourceTypeValue.isObject()) {
        auto typeFlagsError = getTypeFlags(exec, resourceTypeValue, trigger.flags, readResourceType);
        if (typeFlagsError)
            return typeFlagsError;
    } else if (!resourceTypeValue.isUndefined())
        return ContentExtensionError::JSONInvalidTriggerFlagsArray;

    const JSValue loadTypeValue = triggerObject.get(&exec, Identifier::fromString(&exec, "load-type"));
    if (!exec.hadException() && loadTypeValue.isObject()) {
        auto typeFlagsError = getTypeFlags(exec, loadTypeValue, trigger.flags, readLoadType);
        if (typeFlagsError)
            return typeFlagsError;
    } else if (!loadTypeValue.isUndefined())
        return ContentExtensionError::JSONInvalidTriggerFlagsArray;

    const JSValue ifDomain = triggerObject.get(&exec, Identifier::fromString(&exec, "if-domain"));
    if (!exec.hadException() && ifDomain.isObject()) {
        auto ifDomainError = getDomainList(exec, asObject(ifDomain), trigger.domains);
        if (ifDomainError)
            return ifDomainError;
        if (trigger.domains.isEmpty())
            return ContentExtensionError::JSONInvalidDomainList;
        ASSERT(trigger.domainCondition == Trigger::DomainCondition::None);
        trigger.domainCondition = Trigger::DomainCondition::IfDomain;
    } else if (!ifDomain.isUndefined())
        return ContentExtensionError::JSONInvalidDomainList;
    
    const JSValue unlessDomain = triggerObject.get(&exec, Identifier::fromString(&exec, "unless-domain"));
    if (!exec.hadException() && unlessDomain.isObject()) {
        if (trigger.domainCondition != Trigger::DomainCondition::None)
            return ContentExtensionError::JSONUnlessAndIfDomain;
        auto unlessDomainError = getDomainList(exec, asObject(unlessDomain), trigger.domains);
        if (unlessDomainError)
            return unlessDomainError;
        if (trigger.domains.isEmpty())
            return ContentExtensionError::JSONInvalidDomainList;
        trigger.domainCondition = Trigger::DomainCondition::UnlessDomain;
    } else if (!unlessDomain.isUndefined())
        return ContentExtensionError::JSONInvalidDomainList;

    return { };
}

static bool isValidSelector(const String& selector)
{
    CSSParserContext context(CSSQuirksMode);
    CSSParser parser(context);
    CSSSelectorList selectorList;
    parser.parseSelector(selector, selectorList);
    return selectorList.isValid();
}

static std::error_code loadAction(ExecState& exec, const JSObject& ruleObject, Action& action, bool& validSelector)
{
    validSelector = true;
    const JSValue actionObject = ruleObject.get(&exec, Identifier::fromString(&exec, "action"));
    if (!actionObject || exec.hadException() || !actionObject.isObject())
        return ContentExtensionError::JSONInvalidAction;

    const JSValue typeObject = actionObject.get(&exec, Identifier::fromString(&exec, "type"));
    if (!typeObject || exec.hadException() || !typeObject.isString())
        return ContentExtensionError::JSONInvalidActionType;

    String actionType = typeObject.toWTFString(&exec);

    if (actionType == "block")
        action = ActionType::BlockLoad;
    else if (actionType == "ignore-previous-rules")
        action = ActionType::IgnorePreviousRules;
    else if (actionType == "block-cookies")
        action = ActionType::BlockCookies;
    else if (actionType == "css-display-none") {
        JSValue selector = actionObject.get(&exec, Identifier::fromString(&exec, "selector"));
        if (!selector || exec.hadException() || !selector.isString())
            return ContentExtensionError::JSONInvalidCSSDisplayNoneActionType;

        String s = selector.toWTFString(&exec);
        if (!isValidSelector(s)) {
            // Skip rules with invalid selectors to be backwards-compatible.
            validSelector = false;
            return { };
        }
        action = Action(ActionType::CSSDisplayNoneSelector, s);
    } else if (actionType == "make-https") {
        action = ActionType::MakeHTTPS;
    } else
        return ContentExtensionError::JSONInvalidActionType;

    return { };
}

static std::error_code loadRule(ExecState& exec, const JSObject& ruleObject, Vector<ContentExtensionRule>& ruleList)
{
    Trigger trigger;
    auto triggerError = loadTrigger(exec, ruleObject, trigger);
    if (triggerError)
        return triggerError;

    Action action;
    bool validSelector;
    auto actionError = loadAction(exec, ruleObject, action, validSelector);
    if (actionError)
        return actionError;

    if (validSelector)
        ruleList.append(ContentExtensionRule(trigger, action));
    return { };
}

static std::error_code loadEncodedRules(ExecState& exec, const String& rules, Vector<ContentExtensionRule>& ruleList)
{
    // FIXME: JSONParse should require callbacks instead of an ExecState.
    const JSValue decodedRules = JSONParse(&exec, rules);

    if (exec.hadException() || !decodedRules)
        return ContentExtensionError::JSONInvalid;

    if (!decodedRules.isObject())
        return ContentExtensionError::JSONTopLevelStructureNotAnObject;

    const JSObject* topLevelObject = decodedRules.toObject(&exec);
    if (!topLevelObject || exec.hadException())
        return ContentExtensionError::JSONTopLevelStructureNotAnObject;
    
    if (!isJSArray(topLevelObject))
        return ContentExtensionError::JSONTopLevelStructureNotAnArray;

    const JSArray* topLevelArray = jsCast<const JSArray*>(topLevelObject);

    Vector<ContentExtensionRule> localRuleList;

    unsigned length = topLevelArray->length();
    const unsigned maxRuleCount = 50000;
    if (length > maxRuleCount)
        return ContentExtensionError::JSONTooManyRules;
    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = topLevelArray->getIndex(&exec, i);
        if (exec.hadException() || !value)
            return ContentExtensionError::JSONInvalidObjectInTopLevelArray;

        const JSObject* ruleObject = value.toObject(&exec);
        if (!ruleObject || exec.hadException())
            return ContentExtensionError::JSONInvalidRule;

        auto error = loadRule(exec, *ruleObject, localRuleList);
        if (error)
            return error;
    }

    ruleList = WTFMove(localRuleList);
    return { };
}

std::error_code parseRuleList(const String& rules, Vector<ContentExtensionRule>& ruleList)
{
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double loadExtensionStartTime = monotonicallyIncreasingTime();
#endif
    RefPtr<VM> vm = VM::create();

    JSLockHolder locker(vm.get());
    JSGlobalObject* globalObject = JSGlobalObject::create(*vm, JSGlobalObject::createStructure(*vm, jsNull()));

    ExecState* exec = globalObject->globalExec();
    auto error = loadEncodedRules(*exec, rules, ruleList);

    vm = nullptr;

    if (error)
        return error;

    if (ruleList.isEmpty())
        return ContentExtensionError::JSONContainsNoRules;

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double loadExtensionEndTime = monotonicallyIncreasingTime();
    dataLogF("Time spent loading extension %f\n", (loadExtensionEndTime - loadExtensionStartTime));
#endif

    return { };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
