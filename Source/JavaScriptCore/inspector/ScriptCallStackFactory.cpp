/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "CatchScope.h"
#include "CodeBlock.h"
#include "Exception.h"
#include "JSCJSValue.h"
#include "JSCInlines.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "StackVisitor.h"
#include "StrongInlines.h"
#include <wtf/text/WTFString.h>

namespace Inspector {

using namespace JSC;

class CreateScriptCallStackFunctor {
public:
    CreateScriptCallStackFunctor(bool needToSkipAFrame, Vector<ScriptCallFrame>& frames, size_t remainingCapacity)
        : m_needToSkipAFrame(needToSkipAFrame)
        , m_frames(frames)
        , m_remainingCapacityForFrameCapture(remainingCapacity)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (m_needToSkipAFrame) {
            m_needToSkipAFrame = false;
            return StackVisitor::Continue;
        }

        if (m_remainingCapacityForFrameCapture) {
            unsigned line;
            unsigned column;
            visitor->computeLineAndColumn(line, column);
            m_frames.append(ScriptCallFrame(visitor->functionName(), visitor->sourceURL(), static_cast<SourceID>(visitor->sourceID()), line, column));

            m_remainingCapacityForFrameCapture--;
            return StackVisitor::Continue;
        }

        return StackVisitor::Done;
    }

private:
    mutable bool m_needToSkipAFrame;
    Vector<ScriptCallFrame>& m_frames;
    mutable size_t m_remainingCapacityForFrameCapture;
};

Ref<ScriptCallStack> createScriptCallStack(JSC::JSGlobalObject* globalObject, size_t maxStackSize)
{
    if (!globalObject)
        return ScriptCallStack::create();

    JSLockHolder locker(globalObject);
    Vector<ScriptCallFrame> frames;

    VM& vm = globalObject->vm();
    CallFrame* frame = vm.topCallFrame;
    if (!frame)
        return ScriptCallStack::create();
    CreateScriptCallStackFunctor functor(false, frames, maxStackSize);
    frame->iterate(vm, functor);

    return ScriptCallStack::create(frames);
}

Ref<ScriptCallStack> createScriptCallStackForConsole(JSC::JSGlobalObject* globalObject, size_t maxStackSize)
{
    if (!globalObject)
        return ScriptCallStack::create();

    JSLockHolder locker(globalObject);
    Vector<ScriptCallFrame> frames;

    VM& vm = globalObject->vm();
    CallFrame* frame = vm.topCallFrame;
    if (!frame)
        return ScriptCallStack::create();
    CreateScriptCallStackFunctor functor(true, frames, maxStackSize);
    frame->iterate(vm, functor);

    if (frames.isEmpty()) {
        CreateScriptCallStackFunctor functor(false, frames, maxStackSize);
        frame->iterate(vm, functor);
    }

    return ScriptCallStack::create(frames);
}

static bool extractSourceInformationFromException(JSC::JSGlobalObject* globalObject, JSObject* exceptionObject, int* lineNumber, int* columnNumber, String* sourceURL)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    // FIXME: <http://webkit.org/b/115087> Web Inspector: Should not need to evaluate JavaScript handling exceptions
    JSValue lineValue = exceptionObject->getDirect(vm, Identifier::fromString(vm, "line"));
    JSValue columnValue = exceptionObject->getDirect(vm, Identifier::fromString(vm, "column"));
    JSValue sourceURLValue = exceptionObject->getDirect(vm, Identifier::fromString(vm, "sourceURL"));
    
    bool result = false;
    if (lineValue && lineValue.isNumber()
        && sourceURLValue && sourceURLValue.isString()) {
        *lineNumber = int(lineValue.toNumber(globalObject));
        *columnNumber = columnValue && columnValue.isNumber() ? int(columnValue.toNumber(globalObject)) : 0;
        *sourceURL = sourceURLValue.toWTFString(globalObject);
        result = true;
    } else if (ErrorInstance* error = jsDynamicCast<ErrorInstance*>(vm, exceptionObject)) {
        unsigned unsignedLine;
        unsigned unsignedColumn;
        result = getLineColumnAndSource(error->stackTrace(), unsignedLine, unsignedColumn, *sourceURL);
        *lineNumber = static_cast<int>(unsignedLine);
        *columnNumber = static_cast<int>(unsignedColumn);
    }
    
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
        unsigned line;
        unsigned column;
        stackTrace[i].computeLineAndColumn(line, column);
        String functionName = stackTrace[i].functionName(vm);
        frames.append(ScriptCallFrame(functionName, stackTrace[i].sourceURL(), static_cast<SourceID>(stackTrace[i].sourceID()), line, column));
    }

    // Fallback to getting at least the line and sourceURL from the exception object if it has values and the exceptionStack doesn't.
    if (exception->value().isObject()) {
        JSObject* exceptionObject = exception->value().toObject(globalObject);
        ASSERT(exceptionObject);
        int lineNumber;
        int columnNumber;
        String exceptionSourceURL;
        if (!frames.size()) {
            if (extractSourceInformationFromException(globalObject, exceptionObject, &lineNumber, &columnNumber, &exceptionSourceURL))
                frames.append(ScriptCallFrame(String(), exceptionSourceURL, noSourceID, lineNumber, columnNumber));
        } else {
            // FIXME: The typical stack trace will have a native frame at the top, and consumers of
            // this code already know this (see JSDOMExceptionHandling.cpp's reportException, for
            // example - it uses firstNonNativeCallFrame). This looks like it splats something else
            // over it. That something else is probably already at stackTrace[1].
            // https://bugs.webkit.org/show_bug.cgi?id=176663
            if (!stackTrace[0].hasLineAndColumnInfo() || stackTrace[0].sourceURL().isEmpty()) {
                const ScriptCallFrame& firstCallFrame = frames.first();
                if (extractSourceInformationFromException(globalObject, exceptionObject, &lineNumber, &columnNumber, &exceptionSourceURL))
                    frames[0] = ScriptCallFrame(firstCallFrame.functionName(), exceptionSourceURL, stackTrace[0].sourceID(), lineNumber, columnNumber);
            }
        }
    }

    return ScriptCallStack::create(frames);
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
