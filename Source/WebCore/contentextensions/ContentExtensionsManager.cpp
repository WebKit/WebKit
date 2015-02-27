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
#include "ContentExtensionsManager.h"

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

namespace ExtensionsManager {

static bool loadTrigger(ExecState& exec, JSObject& ruleObject, ContentExtensionRule::Trigger& trigger)
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

    JSValue urlFilterCaseObject = triggerObject.get(&exec, Identifier(&exec, "url-filter-is-case-sensitive"));
    if (urlFilterCaseObject && !exec.hadException() && urlFilterCaseObject.isBoolean())
        trigger.urlFilterIsCaseSensitive = urlFilterCaseObject.toBoolean(&exec);

    return true;
}

static bool loadAction(ExecState& exec, JSObject& ruleObject, ContentExtensionRule::Action& action)
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
        action.type = ExtensionActionType::BlockLoad;
    else if (actionType == "ignore-previous-rules")
        action.type = ExtensionActionType::IgnorePreviousRules;
    else if (actionType == "block-cookies")
        action.type = ExtensionActionType::BlockCookies;
    else if (actionType != "block" && actionType != "") {
        WTFLogAlways("Unrecognized action: \"%s\"", actionType.utf8().data());
        return false;
    }

    return true;
}

static void loadRule(ExecState& exec, JSObject& ruleObject, Vector<ContentExtensionRule>& ruleList)
{
    ContentExtensionRule::Trigger trigger;
    if (!loadTrigger(exec, ruleObject, trigger))
        return;

    ContentExtensionRule::Action action;
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

Vector<ContentExtensionRule> createRuleList(const String& rules)
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
    dataLogF("Time spent loading extension %s: %f\n", identifier.utf8().data(), (loadExtensionEndTime - loadExtensionStartTime));
#endif

    return ruleList;
}

} // namespace ExtensionsManager
} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
