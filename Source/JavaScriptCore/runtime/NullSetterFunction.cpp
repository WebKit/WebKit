/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include "NullSetterFunction.h"

#include "CodeBlock.h"
#include "JSCInlines.h"

namespace JSC {

const ClassInfo NullSetterFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(NullSetterFunction) };


#if ASSERT_ENABLED

class GetCallerStrictnessFunctor {
public:
    GetCallerStrictnessFunctor()
        : m_iterations(0)
        , m_callerIsStrict(false)
    {
    }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        ++m_iterations;
        if (m_iterations < 2)
            return IterationStatus::Continue;

        CodeBlock* codeBlock = visitor->codeBlock();
        // This does not take into account that we might have an strict opcode in a non-strict context, but that's
        // ok since we assert below that this function should never be called from any kind strict context.
        m_callerIsStrict = codeBlock && codeBlock->ownerExecutable()->isInStrictContext();
        return IterationStatus::Done;
    }

    bool callerIsStrict() const { return m_callerIsStrict; }

private:
    mutable int m_iterations;
    mutable bool m_callerIsStrict;
};

static bool callerIsStrict(VM& vm, CallFrame* callFrame)
{
    GetCallerStrictnessFunctor iter;
    StackVisitor::visit(callFrame, vm, iter);
    return iter.callerIsStrict();
}

#endif // ASSERT_ENABLED

namespace NullSetterFunctionInternal {

static JSC_DECLARE_HOST_FUNCTION(callReturnUndefined);
static JSC_DECLARE_HOST_FUNCTION(callThrowError);

JSC_DEFINE_HOST_FUNCTION(callReturnUndefined, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
#if !ASSERT_ENABLED
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(callFrame);
#endif
    ASSERT(!callerIsStrict(globalObject->vm(), callFrame));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(callThrowError, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // This function is only called from IC. And we do not want to include this frame in Error's stack.
    constexpr bool useCurrentFrame = false;
    throwException(globalObject, scope, ErrorInstance::create(vm, globalObject->errorStructure(ErrorType::TypeError), ReadonlyPropertyWriteError, JSValue(), nullptr, TypeNothing, ErrorType::TypeError, useCurrentFrame));
    return { };
}

}

NullSetterFunction::NullSetterFunction(VM& vm, Structure* structure, ECMAMode ecmaMode)
    : Base(vm, structure, ecmaMode.isStrict() ? NullSetterFunctionInternal::callThrowError : NullSetterFunctionInternal::callReturnUndefined, nullptr)
{
}

}
