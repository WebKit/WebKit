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

static bool getTypeFlags(ExecState& exec, const JSValue& typeValue, ResourceFlags& flags, uint16_t(*stringToType)(const String&))
{
    if (!typeValue.isObject())
        return true;

    JSObject* object = typeValue.toObject(&exec);
    if (!isJSArray(object)) {
        WTFLogAlways("Invalid trigger flags array");
        return false;
    }

    JSArray* array = jsCast<JSArray*>(object);
    
    unsigned length = array->length();
    for (unsigned i = 0; i < length; ++i) {
        JSValue value = array->getIndex(&exec, i);
        if (exec.hadException() || !value) {
            WTFLogAlways("Invalid object in the trigger flags array.");
            continue;
        }
        
        String name = value.toWTFString(&exec);
        uint16_t type = stringToType(name);
        if (!type) {
            WTFLogAlways("Invalid string in the trigger flags array.");
            continue;
        }
        flags |= type;
    }
    return true;
}
    
static bool loadTrigger(ExecState& exec, JSObject& ruleObject, Trigger& trigger)
{
    JSValue triggerObject = ruleObject.get(&exec, Identifier(&exec, "trigger"));
    if (!triggerObject || exec.hadException() || !triggerObject.isObject()) {
        WTFLogAlways("Invalid trigger object.");
        return false;
    }

    JSValue urlFilterObject = triggerObject.get(&exec, Identifier(&exec, "url-filter"));
    if (!urlFilterObject || exec.hadException() || !urlFilterObject.isString()) {
        WTFLogAlways("Invalid url-filter object.");
        return false;
    }

    String urlFilter = urlFilterObject.toWTFString(&exec);
    if (urlFilter.isEmpty()) {
        WTFLogAlways("Invalid url-filter object. The url is empty.");
        return false;
    }
    trigger.urlFilter = urlFilter;

    JSValue urlFilterCaseValue = triggerObject.get(&exec, Identifier(&exec, "url-filter-is-case-sensitive"));
    if (urlFilterCaseValue && !exec.hadException() && urlFilterCaseValue.isBoolean())
        trigger.urlFilterIsCaseSensitive = urlFilterCaseValue.toBoolean(&exec);

    JSValue resourceTypeValue = triggerObject.get(&exec, Identifier(&exec, "resource-type"));
    if (resourceTypeValue && !exec.hadException() && !getTypeFlags(exec, resourceTypeValue, trigger.flags, readResourceType))
        return false;
    
    JSValue loadTypeValue = triggerObject.get(&exec, Identifier(&exec, "load-type"));
    if (loadTypeValue && !exec.hadException() && !getTypeFlags(exec, loadTypeValue, trigger.flags, readLoadType))
        return false;

    return true;
}

static bool loadAction(ExecState& exec, JSObject& ruleObject, Action& action)
{
    JSValue actionObject = ruleObject.get(&exec, Identifier(&exec, "action"));
    if (!actionObject || exec.hadException() || !actionObject.isObject()) {
        WTFLogAlways("Invalid action object.");
        return false;
    }

    JSValue typeObject = actionObject.get(&exec, Identifier(&exec, "type"));
    if (!typeObject || exec.hadException() || !typeObject.isString()) {
        WTFLogAlways("Invalid url-filter object.");
        return false;
    }

    String actionType = typeObject.toWTFString(&exec);

    if (actionType == "block")
        action = ActionType::BlockLoad;
    else if (actionType == "ignore-previous-rules")
        action = ActionType::IgnorePreviousRules;
    else if (actionType == "block-cookies")
        action = ActionType::BlockCookies;
    else if (actionType == "css-display-none") {
        JSValue selector = actionObject.get(&exec, Identifier(&exec, "selector"));
        if (!selector || exec.hadException() || !selector.isString()) {
            WTFLogAlways("css-display-none action type requires a selector");
            return false;
        }
        action = Action(ActionType::CSSDisplayNoneSelector, selector.toWTFString(&exec));
    } else {
        WTFLogAlways("Unrecognized action: \"%s\"", actionType.utf8().data());
        return false;
    }

    return true;
}

static void loadRule(ExecState& exec, JSObject& ruleObject, Vector<ContentExtensionRule>& ruleList)
{
    Trigger trigger;
    if (!loadTrigger(exec, ruleObject, trigger))
        return;

    Action action;
    if (!loadAction(exec, ruleObject, action))
        return;

    ruleList.append(ContentExtensionRule(trigger, action));
}

static Vector<ContentExtensionRule> loadEncodedRules(ExecState& exec, const String& rules)
{
    JSValue decodedRules = JSONParse(&exec, rules);

    if (exec.hadException() || !decodedRules) {
        WTFLogAlways("Failed to parse the JSON string.");
        return Vector<ContentExtensionRule>();
    }

    if (decodedRules.isObject()) {
        JSObject* topLevelObject = decodedRules.toObject(&exec);
        if (!topLevelObject || exec.hadException()) {
            WTFLogAlways("Invalid input, the top level structure is not an object.");
            return Vector<ContentExtensionRule>();
        }

        if (!isJSArray(topLevelObject)) {
            WTFLogAlways("Invalid input, the top level object is not an array.");
            return Vector<ContentExtensionRule>();
        }

        Vector<ContentExtensionRule> ruleList;
        JSArray* topLevelArray = jsCast<JSArray*>(topLevelObject);

        unsigned length = topLevelArray->length();
        for (unsigned i = 0; i < length; ++i) {
            JSValue value = topLevelArray->getIndex(&exec, i);
            if (exec.hadException() || !value) {
                WTFLogAlways("Invalid object in the array.");
                continue;
            }

            JSObject* ruleObject = value.toObject(&exec);
            if (!ruleObject || exec.hadException()) {
                WTFLogAlways("Invalid rule");
                continue;
            }
            loadRule(exec, *ruleObject, ruleList);
        }
        return ruleList;
    }
    return Vector<ContentExtensionRule>();
}

Vector<ContentExtensionRule> parseRuleList(const String& rules)
{
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double loadExtensionStartTime = monotonicallyIncreasingTime();
#endif
    RefPtr<VM> vm = VM::create();

    JSLockHolder locker(vm.get());
    JSGlobalObject* globalObject = JSGlobalObject::create(*vm, JSGlobalObject::createStructure(*vm, jsNull()));

    ExecState* exec = globalObject->globalExec();
    Vector<ContentExtensionRule> ruleList = loadEncodedRules(*exec, rules);

    vm.clear();

    if (ruleList.isEmpty())
        WTFLogAlways("Empty extension.");

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double loadExtensionEndTime = monotonicallyIncreasingTime();
    dataLogF("Time spent loading extension %f\n", (loadExtensionEndTime - loadExtensionStartTime));
#endif

    return ruleList;
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
