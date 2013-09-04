/*
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

#include "InspectorInstrumentation.h"
#include "JSDOMBinding.h"
#include "JSMainThreadExecState.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "ScriptCallStack.h"
#include "ScriptValue.h"
#include <interpreter/CallFrame.h>
#include <interpreter/CallFrameInlines.h>
#include <interpreter/StackIterator.h>
#include <runtime/ArgList.h>
#include <runtime/JSCJSValue.h>
#include <runtime/JSFunction.h>
#include <runtime/VM.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

class ScriptExecutionContext;

class CreateScriptCallStackFunctor {
public:
    CreateScriptCallStackFunctor(Vector<ScriptCallFrame>& frames,  size_t remainingCapacity)
        : m_frames(frames)
        , m_remainingCapacityForFrameCapture(remainingCapacity)
    {
    }

    StackIterator::Status operator()(StackIterator& iter)
    {
        if (m_remainingCapacityForFrameCapture) {
            unsigned line;
            unsigned column;
            iter->computeLineAndColumn(line, column);
            m_frames.append(ScriptCallFrame(iter->functionName(), iter->sourceURL(), line, column));

            m_remainingCapacityForFrameCapture--;
            return StackIterator::Continue;
        }
        return StackIterator::Done;
    }

private:
    Vector<ScriptCallFrame>& m_frames;
    size_t m_remainingCapacityForFrameCapture;
};

PassRefPtr<ScriptCallStack> createScriptCallStack(size_t maxStackSize, bool emptyIsAllowed)
{
    Vector<ScriptCallFrame> frames;
    if (JSC::ExecState* exec = JSMainThreadExecState::currentState()) {
        CallFrame* frame = exec->vm().topCallFrame;
        CreateScriptCallStackFunctor functor(frames, maxStackSize);
        StackIterator iter = frame->begin();
        iter.iterate(functor);
    }
    if (frames.isEmpty() && !emptyIsAllowed) {
        // No frames found. It may happen in the case where
        // a bound function is called from native code for example.
        // Fallback to setting lineNumber to 0, and source and function name to "undefined".
        frames.append(ScriptCallFrame("undefined", "undefined", 0, 0));
    }
    return ScriptCallStack::create(frames);
}

class CreateScriptCallStackForConsoleFunctor {
public:
    CreateScriptCallStackForConsoleFunctor(bool needToSkipAFrame,  size_t remainingCapacity, Vector<ScriptCallFrame>& frames)
        : m_needToSkipAFrame(needToSkipAFrame)
        , m_remainingCapacityForFrameCapture(remainingCapacity)
        , m_frames(frames)
    {
    }

    StackIterator::Status operator()(StackIterator& iter)
    {
        if (m_needToSkipAFrame) {
            m_needToSkipAFrame = false;
            return StackIterator::Continue;
        }

        if (m_remainingCapacityForFrameCapture) {
            // This early exit is necessary to maintain our old behaviour
            // but the stack trace we produce now is complete and handles all
            // ways in which code may be running
            if (!iter->callee() && m_frames.size())
                return StackIterator::Done;

            unsigned line;
            unsigned column;
            iter->computeLineAndColumn(line, column);
            m_frames.append(ScriptCallFrame(iter->functionName(), iter->sourceURL(), line, column));

            m_remainingCapacityForFrameCapture--;
            return StackIterator::Continue;
        }
        return StackIterator::Done;
    }

private:
    bool m_needToSkipAFrame;
    size_t m_remainingCapacityForFrameCapture;
    Vector<ScriptCallFrame>& m_frames;
};

PassRefPtr<ScriptCallStack> createScriptCallStack(JSC::ExecState* exec, size_t maxStackSize)
{
    Vector<ScriptCallFrame> frames;
    ASSERT(exec);
    CallFrame* frame = exec->vm().topCallFrame;
    StackIterator iter = frame->begin();
    size_t numberOfFrames = iter.numberOfFrames();
    CreateScriptCallStackForConsoleFunctor functor(numberOfFrames > 1, maxStackSize, frames);
    iter.iterate(functor);
    return ScriptCallStack::create(frames);
}

PassRefPtr<ScriptCallStack> createScriptCallStackFromException(JSC::ExecState* exec, JSC::JSValue& exception, size_t maxStackSize)
{
    Vector<ScriptCallFrame> frames;
    RefCountedArray<StackFrame> stackTrace = exec->vm().exceptionStack();
    for (size_t i = 0; i < stackTrace.size() && i < maxStackSize; i++) {
        if (!stackTrace[i].callee && frames.size())
            break;

        String functionName = stackTrace[i].friendlyFunctionName(exec);
        unsigned line;
        unsigned column;
        stackTrace[i].computeLineAndColumn(line, column);
        frames.append(ScriptCallFrame(functionName, stackTrace[i].sourceURL, line, column));
    }

    // FIXME: <http://webkit.org/b/115087> Web Inspector: WebCore::reportException should not evaluate JavaScript handling exceptions
    // Fallback to getting at least the line and sourceURL from the exception if it has values and the exceptionStack doesn't.
    if (frames.size() > 0) {
        const ScriptCallFrame& firstCallFrame = frames.first();
        JSObject* exceptionObject = exception.toObject(exec);
        if (exception.isObject() && firstCallFrame.sourceURL().isEmpty()) {
            JSValue lineValue = exceptionObject->getDirect(exec->vm(), Identifier(exec, "line"));
            int lineNumber = lineValue && lineValue.isNumber() ? int(lineValue.toNumber(exec)) : 0;
            JSValue columnValue = exceptionObject->getDirect(exec->vm(), Identifier(exec, "column"));
            int columnNumber = columnValue && columnValue.isNumber() ? int(columnValue.toNumber(exec)) : 0;
            JSValue sourceURLValue = exceptionObject->getDirect(exec->vm(), Identifier(exec, "sourceURL"));
            String exceptionSourceURL = sourceURLValue && sourceURLValue.isString() ? sourceURLValue.toString(exec)->value(exec) : ASCIILiteral("undefined");
            frames[0] = ScriptCallFrame(firstCallFrame.functionName(), exceptionSourceURL, lineNumber, columnNumber);
        }
    }

    return ScriptCallStack::create(frames);
}

PassRefPtr<ScriptCallStack> createScriptCallStackForConsole(JSC::ExecState* exec)
{
    size_t maxStackSize = 1;
    if (InspectorInstrumentation::hasFrontends()) {
        ScriptExecutionContext* scriptExecutionContext = jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->scriptExecutionContext();
        if (InspectorInstrumentation::consoleAgentEnabled(scriptExecutionContext))
            maxStackSize = ScriptCallStack::maxCallStackSizeToCapture;
    }
    return createScriptCallStack(exec, maxStackSize);
}

PassRefPtr<ScriptArguments> createScriptArguments(JSC::ExecState* exec, unsigned skipArgumentCount)
{
    Vector<ScriptValue> arguments;
    size_t argumentCount = exec->argumentCount();
    for (size_t i = skipArgumentCount; i < argumentCount; ++i)
        arguments.append(ScriptValue(exec->vm(), exec->argument(i)));
    return ScriptArguments::create(exec, arguments);
}

} // namespace WebCore
