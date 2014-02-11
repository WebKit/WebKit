/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "ArgList.h"
#include "CallFrame.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSCInlines.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "ScriptCallStack.h"
#include "ScriptValue.h"
#include "StackVisitor.h"
#include "VM.h"
#include <wtf/RefCountedArray.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace Inspector {

class CreateScriptCallStackFunctor {
public:
    CreateScriptCallStackFunctor(bool needToSkipAFrame, Vector<ScriptCallFrame>& frames, size_t remainingCapacity)
        : m_needToSkipAFrame(needToSkipAFrame)
        , m_frames(frames)
        , m_remainingCapacityForFrameCapture(remainingCapacity)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        if (m_needToSkipAFrame) {
            m_needToSkipAFrame = false;
            return StackVisitor::Continue;
        }

        if (m_remainingCapacityForFrameCapture) {
            unsigned line;
            unsigned column;
            visitor->computeLineAndColumn(line, column);
            m_frames.append(ScriptCallFrame(visitor->functionName(), visitor->sourceURL(), line, column));

            m_remainingCapacityForFrameCapture--;
            return StackVisitor::Continue;
        }

        return StackVisitor::Done;
    }

private:
    bool m_needToSkipAFrame;
    Vector<ScriptCallFrame>& m_frames;
    size_t m_remainingCapacityForFrameCapture;
};

PassRefPtr<ScriptCallStack> createScriptCallStack(JSC::ExecState* exec, size_t maxStackSize, bool emptyIsAllowed)
{
    Vector<ScriptCallFrame> frames;

    if (exec) {
        CallFrame* frame = exec->vm().topCallFrame;
        CreateScriptCallStackFunctor functor(false, frames, maxStackSize);
        frame->iterate(functor);
    }

    if (frames.isEmpty() && !emptyIsAllowed) {
        // No frames found. It may happen in the case where
        // a bound function is called from native code for example.
        // Fallback to setting lineNumber to 0, and source and function name to "undefined".
        frames.append(ScriptCallFrame(ASCIILiteral("undefined"), ASCIILiteral("undefined"), 0, 0));
    }

    return ScriptCallStack::create(frames);
}

PassRefPtr<ScriptCallStack> createScriptCallStack(JSC::ExecState* exec, size_t maxStackSize)
{
    Vector<ScriptCallFrame> frames;

    CallFrame* frame = exec->vm().topCallFrame;
    CreateScriptCallStackFunctor functor(true, frames, maxStackSize);
    frame->iterate(functor);

    if (frames.isEmpty()) {
        CreateScriptCallStackFunctor functor(false, frames, maxStackSize);
        frame->iterate(functor);
    }

    return ScriptCallStack::create(frames);
}

PassRefPtr<ScriptCallStack> createScriptCallStackForConsole(JSC::ExecState* exec)
{
    // FIXME: Caller should use createScriptCallStack alternative with the exec and appropriate max.
    return createScriptCallStack(exec, ScriptCallStack::maxCallStackSizeToCapture);
}

PassRefPtr<ScriptCallStack> createScriptCallStackFromException(JSC::ExecState* exec, JSC::JSValue& exception, size_t maxStackSize)
{
    Vector<ScriptCallFrame> frames;
    RefCountedArray<StackFrame> stackTrace = exec->vm().exceptionStack();
    for (size_t i = 0; i < stackTrace.size() && i < maxStackSize; i++) {
        if (!stackTrace[i].callee && frames.size())
            break;

        unsigned line;
        unsigned column;
        stackTrace[i].computeLineAndColumn(line, column);
        String functionName = stackTrace[i].friendlyFunctionName(exec);
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

PassRefPtr<ScriptArguments> createScriptArguments(JSC::ExecState* exec, unsigned skipArgumentCount)
{
    Vector<Deprecated::ScriptValue> arguments;
    size_t argumentCount = exec->argumentCount();
    for (size_t i = skipArgumentCount; i < argumentCount; ++i)
        arguments.append(Deprecated::ScriptValue(exec->vm(), exec->uncheckedArgument(i)));
    return ScriptArguments::create(exec, arguments);
}

} // namespace WebCore
