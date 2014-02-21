/*
 * Copyright (C) 2006, 2007, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSBase.h"
#include "JSBasePrivate.h"

#include "APICast.h"
#include "APIShims.h"
#include "CallFrame.h"
#include "Completion.h"
#include "InitializeThreading.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObject.h"
#include "OpaqueJSString.h"
#include "JSCInlines.h"
#include "SourceCode.h"
#include <wtf/text/StringHash.h>

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectInspectorController.h"
#endif

using namespace JSC;

JSValueRef JSEvaluateScript(JSContextRef ctx, JSStringRef script, JSObjectRef thisObject, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    ExecState* exec = toJS(ctx);
    APIEntryShim entryShim(exec);

    JSObject* jsThisObject = toJS(thisObject);

    startingLineNumber = std::max(1, startingLineNumber);

    // evaluate sets "this" to the global object if it is NULL
    JSGlobalObject* globalObject = exec->vmEntryGlobalObject();
    SourceCode source = makeSource(script->string(), sourceURL->string(), TextPosition(OrdinalNumber::fromOneBasedInt(startingLineNumber), OrdinalNumber::first()));

    JSValue evaluationException;
    JSValue returnValue = evaluate(globalObject->globalExec(), source, jsThisObject, &evaluationException);

    if (evaluationException) {
        if (exception)
            *exception = toRef(exec, evaluationException);
#if ENABLE(REMOTE_INSPECTOR)
        // FIXME: If we have a debugger attached we could learn about ParseError exceptions through
        // ScriptDebugServer::sourceParsed and this path could produce a duplicate warning. The
        // Debugger path is currently ignored by inspector.
        // NOTE: If we don't have a debugger, this SourceCode will be forever lost to the inspector.
        // We could stash it in the inspector in case an inspector is ever opened.
        globalObject->inspectorController().reportAPIException(exec, evaluationException);
#endif
        return 0;
    }

    if (returnValue)
        return toRef(exec, returnValue);

    // happens, for example, when the only statement is an empty (';') statement
    return toRef(exec, jsUndefined());
}

bool JSCheckScriptSyntax(JSContextRef ctx, JSStringRef script, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    ExecState* exec = toJS(ctx);
    APIEntryShim entryShim(exec);

    startingLineNumber = std::max(1, startingLineNumber);

    SourceCode source = makeSource(script->string(), sourceURL->string(), TextPosition(OrdinalNumber::fromOneBasedInt(startingLineNumber), OrdinalNumber::first()));
    
    JSValue syntaxException;
    bool isValidSyntax = checkSyntax(exec->vmEntryGlobalObject()->globalExec(), source, &syntaxException);

    if (!isValidSyntax) {
        if (exception)
            *exception = toRef(exec, syntaxException);
#if ENABLE(REMOTE_INSPECTOR)
        exec->vmEntryGlobalObject()->inspectorController().reportAPIException(exec, syntaxException);
#endif
        return false;
    }

    return true;
}

void JSGarbageCollect(JSContextRef ctx)
{
    // We used to recommend passing NULL as an argument here, which caused the only heap to be collected.
    // As there is no longer a shared heap, the previously recommended usage became a no-op (but the GC
    // will happen when the context group is destroyed).
    // Because the function argument was originally ignored, some clients may pass their released context here,
    // in which case there is a risk of crashing if another thread performs GC on the same heap in between.
    if (!ctx)
        return;

    ExecState* exec = toJS(ctx);
    APIEntryShim entryShim(exec, false);

    exec->vm().heap.reportAbandonedObjectGraph();
}

void JSReportExtraMemoryCost(JSContextRef ctx, size_t size)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return;
    }
    ExecState* exec = toJS(ctx);
    APIEntryShim entryShim(exec);
    exec->vm().heap.reportExtraMemoryCost(size);
}

extern "C" JS_EXPORT void JSSynchronousGarbageCollectForDebugging(JSContextRef);

void JSSynchronousGarbageCollectForDebugging(JSContextRef ctx)
{
    if (!ctx)
        return;

    ExecState* exec = toJS(ctx);
    APIEntryShim entryShim(exec);
    exec->vm().heap.collectAllGarbage();
}

void JSDisableGCTimer(void)
{
    GCActivityCallback::s_shouldCreateGCTimer = false;
}

#if PLATFORM(IOS)
// FIXME: Expose symbols to tell dyld where to find JavaScriptCore on older versions of
// iOS (< 7.0). We should remove these symbols once we no longer need to support such
// versions of iOS. See <rdar://problem/13696872> for more details.
JS_EXPORT extern const char iosInstallName43 __asm("$ld$install_name$os4.3$/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore");
JS_EXPORT extern const char iosInstallName50 __asm("$ld$install_name$os5.0$/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore");
JS_EXPORT extern const char iosInstallName51 __asm("$ld$install_name$os5.1$/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore");
JS_EXPORT extern const char iosInstallName60 __asm("$ld$install_name$os6.0$/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore");
JS_EXPORT extern const char iosInstallName61 __asm("$ld$install_name$os6.1$/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore");

const char iosInstallName43 = 0;
const char iosInstallName50 = 0;
const char iosInstallName51 = 0;
const char iosInstallName60 = 0;
const char iosInstallName61 = 0;
#endif
