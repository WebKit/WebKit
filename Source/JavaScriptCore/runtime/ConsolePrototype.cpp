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
#include "ConsolePrototype.h"

#include "ConsoleClient.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSCInlines.h"
#include "JSConsole.h"
#include "ScriptArguments.h"
#include "ScriptCallStackFactory.h"

namespace JSC {

const ClassInfo ConsolePrototype::s_info = { "ConsolePrototype", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(ConsolePrototype) };

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDebug(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncError(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncLog(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncWarn(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncClear(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDir(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDirXML(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTable(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTrace(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncAssert(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncCount(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfile(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfileEnd(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTime(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeEnd(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeStamp(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroup(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupCollapsed(ExecState*);
static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupEnd(ExecState*);

void ConsolePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.prototypeMap.addPrototype(this);

    // For legacy reasons, console properties are enumerable, writable, deleteable,
    // and all have a length of 0. This may change if Console is standardized.

    JSC_NATIVE_FUNCTION("debug", consoleProtoFuncDebug, None, 0);
    JSC_NATIVE_FUNCTION("error", consoleProtoFuncError, None, 0);
    JSC_NATIVE_FUNCTION("log", consoleProtoFuncLog, None, 0);
    JSC_NATIVE_FUNCTION("info", consoleProtoFuncLog, None, 0); // "info" is an alias of "log".
    JSC_NATIVE_FUNCTION("warn", consoleProtoFuncWarn, None, 0);

    JSC_NATIVE_FUNCTION("clear", consoleProtoFuncClear, None, 0);
    JSC_NATIVE_FUNCTION("dir", consoleProtoFuncDir, None, 0);
    JSC_NATIVE_FUNCTION("dirxml", consoleProtoFuncDirXML, None, 0);
    JSC_NATIVE_FUNCTION("table", consoleProtoFuncTable, None, 0);
    JSC_NATIVE_FUNCTION("trace", consoleProtoFuncTrace, None, 0);
    JSC_NATIVE_FUNCTION("assert", consoleProtoFuncAssert, None, 0);
    JSC_NATIVE_FUNCTION("count", consoleProtoFuncCount, None, 0);
    JSC_NATIVE_FUNCTION("profile", consoleProtoFuncProfile, None, 0);
    JSC_NATIVE_FUNCTION("profileEnd", consoleProtoFuncProfileEnd, None, 0);
    JSC_NATIVE_FUNCTION("time", consoleProtoFuncTime, None, 0);
    JSC_NATIVE_FUNCTION("timeEnd", consoleProtoFuncTimeEnd, None, 0);
    JSC_NATIVE_FUNCTION("timeStamp", consoleProtoFuncTimeStamp, None, 0);
    JSC_NATIVE_FUNCTION("group", consoleProtoFuncGroup, None, 0);
    JSC_NATIVE_FUNCTION("groupCollapsed", consoleProtoFuncGroupCollapsed, None, 0);
    JSC_NATIVE_FUNCTION("groupEnd", consoleProtoFuncGroupEnd, None, 0);
}

static String valueToStringWithUndefinedOrNullCheck(ExecState* exec, JSValue value)
{
    if (value.isUndefinedOrNull())
        return String();
    return value.toString(exec)->value(exec);
}

static EncodedJSValue consoleLogWithLevel(ExecState* exec, MessageLevel level)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->logWithLevel(exec, arguments.release(), level);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDebug(ExecState* exec)
{
    return consoleLogWithLevel(exec, MessageLevel::Debug);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncError(ExecState* exec)
{
    return consoleLogWithLevel(exec, MessageLevel::Error);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncLog(ExecState* exec)
{
    return consoleLogWithLevel(exec, MessageLevel::Log);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncWarn(ExecState* exec)
{
    return consoleLogWithLevel(exec, MessageLevel::Warning);
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncClear(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->clear(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDir(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->dir(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncDirXML(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->dirXML(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTable(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->table(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTrace(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->trace(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncAssert(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    bool condition(exec->argument(0).toBoolean(exec));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 1));
    client->assertCondition(exec, arguments.release(), condition);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncCount(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->count(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfile(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = exec->argumentCount();
    if (argsCount <= 0) {
        client->profile(exec, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(exec, exec->argument(0)));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    client->profile(exec, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncProfileEnd(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = exec->argumentCount();
    if (argsCount <= 0) {
        client->profileEnd(exec, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(exec, exec->argument(0)));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    client->profileEnd(exec, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTime(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    if (exec->argumentCount() < 1)
        return throwVMError(exec, createNotEnoughArgumentsError(exec));

    const String& title(valueToStringWithUndefinedOrNullCheck(exec, exec->argument(0)));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    client->time(exec, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeEnd(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    if (exec->argumentCount() < 1)
        return throwVMError(exec, createNotEnoughArgumentsError(exec));

    const String& title(valueToStringWithUndefinedOrNullCheck(exec, exec->argument(0)));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    client->timeEnd(exec, title);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncTimeStamp(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->timeStamp(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroup(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->group(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupCollapsed(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->groupCollapsed(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL consoleProtoFuncGroupEnd(ExecState* exec)
{
    JSConsole* castedThis = jsDynamicCast<JSConsole*>(exec->hostThisValue());
    if (!castedThis)
        return throwVMTypeError(exec);
    ASSERT_GC_OBJECT_INHERITS(castedThis, JSConsole::info());
    ConsoleClient* client = castedThis->globalObject()->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    RefPtr<Inspector::ScriptArguments> arguments(Inspector::createScriptArguments(exec, 0));
    client->groupEnd(exec, arguments.release());
    return JSValue::encode(jsUndefined());
}

}
