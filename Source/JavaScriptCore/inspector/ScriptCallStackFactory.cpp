/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptCallStackFactory.h"

#include "CodeBlock.h"
#include "Debugger.h"
#include "ExecutableBaseInlines.h"
#include "ImplementationVisibility.h"
#include "InspectorDebuggerAgent.h"
#include "JSCInlines.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "ScriptExecutable.h"
#include <wtf/text/WTFString.h>

namespace Inspector {

using namespace JSC;

class CreateScriptCallStackFunctor {
public:
    CreateScriptCallStackFunctor(JSC::JSGlobalObject* globalObject, bool needToSkipAFrame, size_t remainingCapacity)
        : m_globalObject(globalObject)
        , m_needToSkipAFrame(needToSkipAFrame)
        , m_remainingCapacityForFrameCapture(remainingCapacity)
    {
    }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        if (m_needToSkipAFrame) {
            m_needToSkipAFrame = false;
            return IterationStatus::Continue;
        }

        if (visitor->isImplementationVisibilityPrivate())
            return IterationStatus::Continue;

        if (m_remainingCapacityForFrameCapture) {
            auto lineColumn = visitor->computeLineAndColumn();
            m_frames.append(ScriptCallFrame(visitor->functionName(), visitor->sourceURL(), visitor->preRedirectURL(), visitor->sourceID(), lineColumn));

            m_remainingCapacityForFrameCapture--;
            return IterationStatus::Continue;
        }

        m_truncated = true;
        return IterationStatus::Done;
    }

    Ref<ScriptCallStack> takeStack()
    {
        AsyncStackTrace* parentStackTrace = nullptr;
        if (auto* debugger = m_globalObject->debugger()) {
            if (auto* debuggerAgent = dynamicDowncast<InspectorDebuggerAgent>(debugger->client()))
                parentStackTrace = debuggerAgent->currentParentStackTrace();
        }

        return ScriptCallStack::create(WTFMove(m_frames), m_truncated, parentStackTrace);
    }

private:
    JSGlobalObject* m_globalObject;

    mutable bool m_needToSkipAFrame;

    mutable Vector<ScriptCallFrame> m_frames;
    mutable bool m_truncated { false };

    mutable size_t m_remainingCapacityForFrameCapture;
};

Ref<ScriptCallStack> createScriptCallStack(JSC::JSGlobalObject* globalObject, size_t maxStackSize)
{
    if (!globalObject)
        return ScriptCallStack::create();

    JSLockHolder locker(globalObject);

    VM& vm = globalObject->vm();
    CallFrame* frame = vm.topCallFrame;
    if (!frame)
        return ScriptCallStack::create();

    CreateScriptCallStackFunctor functorIncludingFirstFrame(globalObject, false, maxStackSize);
    StackVisitor::visit(frame, vm, functorIncludingFirstFrame);
    return functorIncludingFirstFrame.takeStack();
}

Ref<ScriptCallStack> createScriptCallStackForConsole(JSC::JSGlobalObject* globalObject, size_t maxStackSize)
{
    if (!globalObject)
        return ScriptCallStack::create();

    JSLockHolder locker(globalObject);

    VM& vm = globalObject->vm();
    CallFrame* frame = vm.topCallFrame;
    if (!frame)
        return ScriptCallStack::create();

    CreateScriptCallStackFunctor functorSkippingFirstFrame(globalObject, true, maxStackSize);
    StackVisitor::visit(frame, vm, functorSkippingFirstFrame);
    auto stack = functorSkippingFirstFrame.takeStack();
    if (!stack->size()) {
        CreateScriptCallStackFunctor functorIncludingFirstFrame(globalObject, false, maxStackSize);
        StackVisitor::visit(frame, vm, functorIncludingFirstFrame);
        stack = functorIncludingFirstFrame.takeStack();
    }
    return stack;
}

static bool extractSourceInformationFromException(JSC::JSGlobalObject* globalObject, JSObject* exceptionObject, LineColumn* lineColumn, String* sourceURL)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    // FIXME: <http://webkit.org/b/115087> Web Inspector: Should not need to evaluate JavaScript handling exceptions
    JSValue lineValue = exceptionObject->getDirect(vm, Identifier::fromString(vm, "line"_s));
    JSValue columnValue = exceptionObject->getDirect(vm, Identifier::fromString(vm, "column"_s));
    JSValue sourceURLValue = exceptionObject->getDirect(vm, Identifier::fromString(vm, "sourceURL"_s));

    bool result = false;
    if (lineValue && lineValue.isNumber()
        && sourceURLValue && sourceURLValue.isString()) {
        lineColumn->line = int(lineValue.toNumber(globalObject));
        lineColumn->column = columnValue && columnValue.isNumber() ? int(columnValue.toNumber(globalObject)) : 0;
        *sourceURL = sourceURLValue.toWTFString(globalObject);
        result = true;
    } else if (ErrorInstance* error = jsDynamicCast<ErrorInstance*>(exceptionObject))
        result = getLineColumnAndSource(vm, error->stackTrace(), *lineColumn, *sourceURL);

    if (sourceURL->isEmpty())
        *sourceURL = "undefined"_s;

    scope.clearException();
    return result;
}

Ref<ScriptCallStack> createScriptCallStackFromException(JSC::JSGlobalObject* globalObject, JSC::Exception* exception, size_t maxStackSize)
{
    Vector<ScriptCallFrame> frames;
    auto& stackTrace = exception->stack();
    VM& vm = globalObject->vm();
    for (size_t i = 0; i < stackTrace.size() && i < maxStackSize; i++) {
        auto lineColumn = stackTrace[i].computeLineAndColumn();
        String functionName = stackTrace[i].functionName(vm);
        frames.append(ScriptCallFrame(functionName, stackTrace[i].sourceURL(vm), stackTrace[i].sourceID(), lineColumn));
    }

    // Fallback to getting at least the line and sourceURL from the exception object if it has values and the exceptionStack doesn't.
    if (exception->value().isObject()) {
        JSObject* exceptionObject = exception->value().toObject(globalObject);
        ASSERT(exceptionObject);
        LineColumn lineColumn;
        String exceptionSourceURL;
        if (!frames.size()) {
            if (extractSourceInformationFromException(globalObject, exceptionObject, &lineColumn, &exceptionSourceURL))
                frames.append(ScriptCallFrame(String(), exceptionSourceURL, noSourceID, lineColumn));
        } else {
            // FIXME: The typical stack trace will have a native frame at the top, and consumers of
            // this code already know this (see JSDOMExceptionHandling.cpp's reportException, for
            // example - it uses firstNonNativeCallFrame). This looks like it splats something else
            // over it. That something else is probably already at stackTrace[1].
            // https://bugs.webkit.org/show_bug.cgi?id=176663
            if (!stackTrace[0].hasLineAndColumnInfo() || stackTrace[0].sourceURL(vm).isEmpty()) {
                const ScriptCallFrame& firstCallFrame = frames.first();
                if (extractSourceInformationFromException(globalObject, exceptionObject, &lineColumn, &exceptionSourceURL))
                    frames[0] = ScriptCallFrame(firstCallFrame.functionName(), exceptionSourceURL, stackTrace[0].sourceID(), lineColumn);
            }
        }
    }

    AsyncStackTrace* parentStackTrace = nullptr;
    if (auto* debugger = globalObject->debugger()) {
        if (auto* debuggerAgent = dynamicDowncast<InspectorDebuggerAgent>(debugger->client()))
            parentStackTrace = debuggerAgent->currentParentStackTrace();
    }
    return ScriptCallStack::create(WTFMove(frames), stackTrace.size() > maxStackSize, parentStackTrace);
}

Ref<ScriptArguments> createScriptArguments(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame, unsigned skipArgumentCount)
{
    VM& vm = globalObject->vm();
    Vector<JSC::Strong<JSC::Unknown>> arguments;
    size_t argumentCount = callFrame->argumentCount();
    for (size_t i = skipArgumentCount; i < argumentCount; ++i)
        arguments.append({ vm, callFrame->uncheckedArgument(i) });
    return ScriptArguments::create(globalObject, WTFMove(arguments));
}

} // namespace Inspector
