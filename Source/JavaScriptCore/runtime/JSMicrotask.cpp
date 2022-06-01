/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSMicrotask.h"

#include "CatchScope.h"
#include "Debugger.h"
#include "DeferTermination.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "Microtask.h"
#include "StrongInlines.h"
#include "VMTrapsInlines.h"

namespace JSC {

static inline void runMicrotaskWithCount(JSC::Microtask* microtask, JSGlobalObject* globalObject, JSValue job, MarkedArgumentBuffer&& handlerArguments)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto handlerCallData = JSC::getCallData(job);
    ASSERT(handlerCallData.type != CallData::Type::None);

    if (UNLIKELY(globalObject->hasDebugger()))
        globalObject->debugger()->willRunMicrotask(globalObject, *microtask);

    profiledCall(globalObject, ProfilingReason::Microtask, job, handlerCallData, jsUndefined(), handlerArguments);
    scope.clearException();

    if (UNLIKELY(globalObject->hasDebugger()))
        globalObject->debugger()->didRunMicrotask(globalObject, *microtask);
}

class JSMicrotask final : public Microtask {
public:
    static constexpr unsigned maxArguments = 4;
    JSMicrotask(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
    {
        m_job.set(vm, job);
        if (argument0 && !argument0.isUndefined())
            m_arguments[0].set(vm, argument0);
        if (argument1 && !argument1.isUndefined())
            m_arguments[1].set(vm, argument1);
        if (argument2 && !argument2.isUndefined())
            m_arguments[2].set(vm, argument2);
        if (argument3 && !argument3.isUndefined())
            m_arguments[3].set(vm, argument3);
    }

private:
    void run(JSGlobalObject*) final;

    Strong<Unknown> m_job;
    Strong<Unknown> m_arguments[maxArguments];
};

class JSMicrotaskNoArguments final : public Microtask {
    static constexpr unsigned maxArguments = 0;

public:
    JSMicrotaskNoArguments(VM& vm, JSValue job)
    {
        m_job.set(vm, job);
    }

private:
    void run(JSGlobalObject* globalObject)
    {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);

        JSC::JSValue job = m_job.get();
        auto handlerCallData = JSC::getCallData(job);
        ASSERT(handlerCallData.type != CallData::Type::None);

        MarkedArgumentBuffer handlerArguments;

        if (UNLIKELY(globalObject->hasDebugger()))
            globalObject->debugger()->willRunMicrotask(globalObject, *this);

        profiledCall(globalObject, ProfilingReason::Microtask, job, handlerCallData, jsUndefined(), handlerArguments);
        scope.clearException();

        if (UNLIKELY(globalObject->hasDebugger()))
            globalObject->debugger()->didRunMicrotask(globalObject, *this);
    }

    Strong<Unknown> m_job;
};

class JSMicrotask1Argument final : public Microtask {
    static constexpr unsigned maxArguments = 2;

public:
    JSMicrotask1Argument(VM& vm, JSValue job, JSValue argument0)
    {
        m_args[0].set(vm, job);
        m_args[1].set(vm, argument0);
    }

private:
    void run(JSGlobalObject* globalObject)
    {
        MarkedArgumentBuffer handlerArguments;
        JSC::JSValue job = m_args[0].get();
        handlerArguments.append(m_args[1].get());
        m_args[0].clear();
        m_args[1].clear();
        runMicrotaskWithCount(this, globalObject, job, WTFMove(handlerArguments));
    }

    Strong<Unknown> m_args[maxArguments];
};

class JSMicrotask2Argument final : public Microtask {
    static constexpr unsigned maxArguments = 3;

public:
    JSMicrotask2Argument(VM& vm, JSValue job, JSValue argument0, JSValue argument1)
    {
        m_args[0].set(vm, job);
        m_args[1].set(vm, argument0);
        m_args[2].set(vm, argument1);
    }

private:
    void run(JSGlobalObject* globalObject)
    {
        MarkedArgumentBuffer handlerArguments;
        JSValue job = m_args[0].get();
        m_args[0].clear();

        handlerArguments.append(m_args[1].get());
        m_args[1].clear();

        handlerArguments.append(m_args[2].get());
        m_args[2].clear();

        runMicrotaskWithCount(this, globalObject, job, WTFMove(handlerArguments));
    }

    Strong<Unknown> m_args[maxArguments];
};

class JSMicrotask3Argument final : public Microtask {
    static constexpr unsigned maxArguments = 4;

public:
    JSMicrotask3Argument(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2)
    {
        m_args[0].set(vm, job);
        m_args[1].set(vm, argument0);
        m_args[2].set(vm, argument1);
        m_args[3].set(vm, argument2);
    }

private:
    void run(JSGlobalObject* globalObject)
    {
        MarkedArgumentBuffer handlerArguments;
        JSValue job = m_args[0].get();
        m_args[0].clear();
        handlerArguments.append(m_args[1].get());
        m_args[1].clear();
        handlerArguments.append(m_args[2].get());
        m_args[2].clear();
        handlerArguments.append(m_args[3].get());
        m_args[3].clear();

        runMicrotaskWithCount(this, globalObject, job, WTFMove(handlerArguments));
    }

    Strong<Unknown> m_args[maxArguments];
};

Ref<Microtask> createJSMicrotask(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
{
    if (!argument3 || argument3.isUndefined()) {
        if (!argument2 || argument2.isUndefined()) {
            if (!argument1 || argument1.isUndefined()) {
                if (!argument0 || argument0.isUndefined()) {
                    return adoptRef(*new JSMicrotaskNoArguments(vm, job));
                }
                return adoptRef(*new JSMicrotask1Argument(vm, job, argument0));
            }
            return adoptRef(*new JSMicrotask2Argument(vm, job, argument0, argument1));
        }
        return adoptRef(*new JSMicrotask3Argument(vm, job, argument0, argument1, argument2));
    }

    return adoptRef(*new JSMicrotask(vm, job, argument0, argument1, argument2, argument3));
}

void runJSMicrotask(JSGlobalObject* globalObject, MicrotaskIdentifier identifier, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
{
    MarkedArgumentBuffer handlerArguments;
    for (unsigned index = 0; index < maxArguments; ++index) {
        JSValue arg = m_arguments[index].get();
        if (!arg)
            break;
        handlerArguments.append(arg);
        m_arguments[index].clear();
    }
    JSC::JSValue job = m_job.get();
    m_job.clear();

    runMicrotaskWithCount(this, globalObject, job, WTFMove(handlerArguments));
}

} // namespace JSC
