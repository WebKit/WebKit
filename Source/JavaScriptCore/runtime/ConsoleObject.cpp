/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ConsoleObject.h"

#include "ConsoleClient.h"
#include "Error.h"
#include "JSCInlines.h"
#include "ScriptArguments.h"
#include "ScriptCallStackFactory.h"

namespace JSC {

static String valueOrDefaultLabelString(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return "default"_s;

    auto value = exec->argument(0);
    if (value.isUndefined())
        return "default"_s;

    return value.toWTFString(exec);
}

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ConsoleObject);

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDebug(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncError(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncLog(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncInfo(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncWarn(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncClear(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDir(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDirXML(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTable(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTrace(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncAssert(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncCount(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncCountReset(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfile(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfileEnd(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTakeHeapSnapshot(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTime(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeLog(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeEnd(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeStamp(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroup(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupCollapsed(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupEnd(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncRecord(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncRecordEnd(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncScreenshot(JSGlobalObject*, CallFrame*);

const ClassInfo ConsoleObject::s_info = { "Console", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ConsoleObject) };

ConsoleObject::ConsoleObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void ConsoleObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    // For legacy reasons, console properties are enumerable, writable, deleteable,
    // and all have a length of 0. This may change if Console is standardized.

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("debug", consoleProtoFuncDebug, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("error", consoleProtoFuncError, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("log", consoleProtoFuncLog, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("info", consoleProtoFuncInfo, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("warn", consoleProtoFuncWarn, static_cast<unsigned>(PropertyAttribute::None), 0);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->clear, consoleProtoFuncClear, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("dir", consoleProtoFuncDir, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("dirxml", consoleProtoFuncDirXML, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("table", consoleProtoFuncTable, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("trace", consoleProtoFuncTrace, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("assert", consoleProtoFuncAssert, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->count, consoleProtoFuncCount, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("countReset", consoleProtoFuncCountReset, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("profile", consoleProtoFuncProfile, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("profileEnd", consoleProtoFuncProfileEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("time", consoleProtoFuncTime, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeLog", consoleProtoFuncTimeLog, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeEnd", consoleProtoFuncTimeEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeStamp", consoleProtoFuncTimeStamp, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("takeHeapSnapshot", consoleProtoFuncTakeHeapSnapshot, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("group", consoleProtoFuncGroup, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("groupCollapsed", consoleProtoFuncGroupCollapsed, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("groupEnd", consoleProtoFuncGroupEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("record", consoleProtoFuncRecord, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("recordEnd", consoleProtoFuncRecordEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("screenshot", consoleProtoFuncScreenshot, static_cast<unsigned>(PropertyAttribute::None), 0);
}

static String valueToStringWithUndefinedOrNullCheck(ExecState* exec, JSValue value)
{
    if (value.isUndefinedOrNull())
        return String();
    return value.toWTFString(exec);
}

static EncodedJSValue consoleLogWithLevel(CallFrame* callFrame, JSGlobalObject* globalObject, MessageLevel level)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->logWithLevel(callFrame, Inspector::createScriptArguments(callFrame, 0), level);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDebug(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return consoleLogWithLevel(callFrame, globalObject, MessageLevel::Debug);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return consoleLogWithLevel(callFrame, globalObject, MessageLevel::Error);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncLog(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return consoleLogWithLevel(callFrame, globalObject, MessageLevel::Log);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncInfo(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return consoleLogWithLevel(callFrame, globalObject, MessageLevel::Info);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncWarn(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return consoleLogWithLevel(callFrame, globalObject, MessageLevel::Warning);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncClear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->clear(callFrame);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDir(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->dir(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDirXML(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->dirXML(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTable(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->table(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTrace(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->trace(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncAssert(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    bool condition = callFrame->argument(0).toBoolean(callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (condition)
        return JSValue::encode(jsUndefined());

    client->assertion(callFrame, Inspector::createScriptArguments(callFrame, 1));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncCount(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->count(callFrame, label);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncCountReset(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->countReset(callFrame, label);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfile(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = callFrame->argumentCount();
    if (!argsCount) {
        client->profile(callFrame, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(callFrame, callFrame->argument(0)));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->profile(callFrame, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfileEnd(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = callFrame->argumentCount();
    if (!argsCount) {
        client->profileEnd(callFrame, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(callFrame, callFrame->argument(0)));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->profileEnd(callFrame, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTakeHeapSnapshot(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = callFrame->argumentCount();
    if (!argsCount) {
        client->takeHeapSnapshot(callFrame, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(callFrame, callFrame->argument(0)));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->takeHeapSnapshot(callFrame, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTime(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->time(callFrame, label);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeLog(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->timeLog(callFrame, label, Inspector::createScriptArguments(callFrame, 1));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeEnd(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->timeEnd(callFrame, label);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeStamp(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->timeStamp(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroup(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->group(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupCollapsed(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->groupCollapsed(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupEnd(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->groupEnd(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncRecord(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->record(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncRecordEnd(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->recordEnd(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncScreenshot(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ConsoleClient* client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->screenshot(callFrame, Inspector::createScriptArguments(callFrame, 0));
    return JSValue::encode(jsUndefined());
}

} // namespace JSC
