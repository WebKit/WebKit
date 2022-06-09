/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CallFrameClosure.h"
#include "ExceptionHelpers.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "ProtoCallFrame.h"
#include "VMEntryScope.h"
#include "VMInlines.h"
#include <wtf/ForbidHeapAllocation.h>

namespace JSC {
    class CachedCall {
        WTF_MAKE_NONCOPYABLE(CachedCall);
        WTF_FORBID_HEAP_ALLOCATION;
    public:
        CachedCall(JSGlobalObject* globalObject, CallFrame* callFrame, JSFunction* function, int argumentCount)
            : m_valid(false)
            , m_vm(globalObject->vm())
            , m_interpreter(m_vm.interpreter)
            , m_entryScope(m_vm, function->scope()->globalObject(m_vm))
        {
            VM& vm = m_entryScope.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            ASSERT(!function->isHostFunctionNonInline());
            if (LIKELY(vm.isSafeToRecurseSoft())) {
                m_arguments.ensureCapacity(argumentCount);
                if (LIKELY(!m_arguments.hasOverflowed()))
                    m_closure = m_interpreter->prepareForRepeatCall(function->jsExecutable(), callFrame, &m_protoCallFrame, function, argumentCount + 1, function->scope(), m_arguments);
                else
                    throwOutOfMemoryError(globalObject, scope);
            } else
                throwStackOverflowError(globalObject, scope);
            m_valid = !scope.exception();
        }
        
        ALWAYS_INLINE JSValue call()
        { 
            ASSERT(m_valid);
            ASSERT(m_arguments.size() == static_cast<size_t>(m_protoCallFrame.argumentCount()));
            return m_interpreter->execute(m_closure);
        }
        void setThis(JSValue v) { m_protoCallFrame.setThisValue(v); }

        void clearArguments() { m_arguments.clear(); }
        void appendArgument(JSValue v) { m_arguments.append(v); }
        bool hasOverflowedArguments() { return m_arguments.hasOverflowed(); }

    private:
        bool m_valid;
        VM& m_vm;
        Interpreter* m_interpreter;
        VMEntryScope m_entryScope;
        ProtoCallFrame m_protoCallFrame;
        MarkedArgumentBuffer m_arguments;
        CallFrameClosure m_closure;
    };
}
