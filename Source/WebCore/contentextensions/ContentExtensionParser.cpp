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

#include "ContentExtensionError.h"
#include "ContentExtensionRule.h"
#include "ContentExtensionsBackend.h"
#include "ContentExtensionsDebugging.h"
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/StructureInlines.h>
#include <JavaScriptCore/VM.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

namespace ContentExtensions {

static std::error_code getTypeFlags(ExecState& exec, const JSValue& typeValue, ResourceFlags& flags, uint16_t (*stringToType)(const String&))
{
    if (!typeValue.isObject())
        return { };

    JSObject* object = typeValue.toObject(&exec);
    if (!isJSArray(object))
        return ContentExtensionError::JSONInvalidTriggerFlagsArray;

    JSArray* array = jsCast<JSArray*>(object);
    
    unsigned length = array->length();
    for (unsigned i = 0; i < length; ++i) {
        JSValue value = array->getIndex(&exec, i);
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
    
static std::error_code loadTrigger(ExecState& exec, JSObject& ruleObject, Trigger& trigger)
{
    JSValue triggerObject = ruleObject.get(&exec, Identifier(&exec, "trigger"));
    if (!triggerObject || exec.hadException() || !triggerObject.isObject())
        return ContentExtensionError::JSONInvalidTrigger;
    
    JSValue urlFilterObject = triggerObject.get(&exec, Identifier(&exec, "url-filter"));
    if (!urlFilterObject || exec.hadException() || !urlFilterObject.isString())
        return ContentExtensionError::JSONInvalidURLFilterInTrigger;

    String urlFilter = urlFilterObject.toWTFString(&exec);
    if (urlFilter.isEmpty())
        return ContentExtensionError::JSONInvalidURLFilterInTrigger;

    trigger.urlFilter = urlFilter;

    JSValue urlFilterCaseValue = triggerObject.get(&exec, Identifier(&exec, "url-filter-is-case-sensitive"));
    if (urlFilterCaseValue && !exec.hadException() && urlFilterCaseValue.isBoolean())
        trigger.urlFilterIsCaseSensitive = urlFilterCaseValue.toBoolean(&exec);

    JSValue resourceTypeValue = triggerObject.get(&exec, Identifier(&exec, "resource-type"));
    if (resourceTypeValue && !exec.hadException()) {
        auto typeFlagsError = getTypeFlags(exec, resourceTypeValue, trigger.flags, readResourceType);
        if (typeFlagsError)
            return typeFlagsError;
    }

    JSValue loadTypeValue = triggerObject.get(&exec, Identifier(&exec, "load-type"));
    if (loadTypeValue && !exec.hadException()) {
        auto typeFlagsError = getTypeFlags(exec, loadTypeValue, trigger.flags, readLoadType);
        if (typeFlagsError)
            return typeFlagsError;
    }

    return { };
}

static std::error_code loadAction(ExecState& exec, JSObject& ruleObject, Action& action)
{
    JSValue actionObject = ruleObject.get(&exec, Identifier(&exec, "action"));
    if (!actionObject || exec.hadException() || !actionObject.isObject())
        return ContentExtensionError::JSONInvalidAction;

    JSValue typeObject = actionObject.get(&exec, Identifier(&exec, "type"));
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
        JSValue selector = actionObject.get(&exec, Identifier(&exec, "selector"));
        if (!selector || exec.hadException() || !selector.isString())
            return ContentExtensionError::JSONInvalidCSSDisplayNoneActionType;

        action = Action(ActionType::CSSDisplayNoneSelector, selector.toWTFString(&exec));
    } else
        return ContentExtensionError::JSONInvalidActionType;

    return { };
}

static std::error_code loadRule(ExecState& exec, JSObject& ruleObject, Vector<ContentExtensionRule>& ruleList)
{
    Trigger trigger;
    auto triggerError = loadTrigger(exec, ruleObject, trigger);
    if (triggerError)
        return triggerError;

    Action action;
    auto actionError = loadAction(exec, ruleObject, action);
    if (actionError)
        return actionError;

    ruleList.append(ContentExtensionRule(trigger, action));
    return { };
}

static std::error_code loadEncodedRules(ExecState& exec, const String& rules, Vector<ContentExtensionRule>& ruleList)
{
    JSValue decodedRules = JSONParse(&exec, rules);

    if (exec.hadException() || !decodedRules)
        return ContentExtensionError::JSONInvalid;

    if (!decodedRules.isObject())
        return ContentExtensionError::JSONTopLevelStructureNotAnObject;

    JSObject* topLevelObject = decodedRules.toObject(&exec);
    if (!topLevelObject || exec.hadException())
        return ContentExtensionError::JSONTopLevelStructureNotAnObject;
    
    if (!isJSArray(topLevelObject))
        return ContentExtensionError::JSONTopLevelStructureNotAnArray;

    JSArray* topLevelArray = jsCast<JSArray*>(topLevelObject);

    Vector<ContentExtensionRule> localRuleList;

    unsigned length = topLevelArray->length();
    for (unsigned i = 0; i < length; ++i) {
        JSValue value = topLevelArray->getIndex(&exec, i);
        if (exec.hadException() || !value)
            return ContentExtensionError::JSONInvalidObjectInTopLevelArray;

        JSObject* ruleObject = value.toObject(&exec);
        if (!ruleObject || exec.hadException())
            return ContentExtensionError::JSONInvalidRule;

        auto error = loadRule(exec, *ruleObject, localRuleList);
        if (error)
            return error;
    }

    ruleList = WTF::move(localRuleList);
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

    vm.clear();

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
