/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ExceptionScope.h>

namespace JSC {

// TopExceptionScope is intended to used at the top of the JS stack when we wouldn't want to propagate
// exceptions further. For example, this is often used where we take the JSLock.
// N.B. Most code should use ThrowScope to do exception handling (including clearing exceptions) as termination
// exceptions mean that almost all catch sites can also rethrow.

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)

class TopExceptionScope : public ExceptionScope {
public:
    JS_EXPORT_PRIVATE TopExceptionScope(VM&, ExceptionEventLocation);
    TopExceptionScope(const TopExceptionScope&) = delete;
    TopExceptionScope(TopExceptionScope&&) = default;

    JS_EXPORT_PRIVATE ~TopExceptionScope();

    void clearException();
    bool clearExceptionExceptTermination();
};

#define DECLARE_TOP_EXCEPTION_SCOPE(vm__) \
    JSC::TopExceptionScope((vm__), JSC::ExceptionEventLocation(EXCEPTION_SCOPE_POSITION_FOR_ASAN(vm__), __FUNCTION__, __FILE__, __LINE__))

#else // not ENABLE(EXCEPTION_SCOPE_VERIFICATION)

class TopExceptionScope : public ExceptionScope {
public:
    ALWAYS_INLINE TopExceptionScope(VM& vm)
        : ExceptionScope(vm)
    { }
    TopExceptionScope(const TopExceptionScope&) = delete;
    TopExceptionScope(TopExceptionScope&&) = default;

    ALWAYS_INLINE void clearException();
    ALWAYS_INLINE bool clearExceptionExceptTermination();
};

#define DECLARE_TOP_EXCEPTION_SCOPE(vm__) \
    JSC::TopExceptionScope((vm__))

#endif // ENABLE(EXCEPTION_SCOPE_VERIFICATION)

ALWAYS_INLINE void TopExceptionScope::clearException()
{
    m_vm.clearException();
}

ALWAYS_INLINE bool TopExceptionScope::clearExceptionExceptTermination()
{
    if (m_vm.hasPendingTerminationException()) [[unlikely]] {
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
        m_vm.exception();
#endif
        return false;
    }
    m_vm.clearException();
    return true;
}

#define CLEAR_AND_RETURN_IF_EXCEPTION(scope__, value__) do { \
        if ((scope__).exception()) [[unlikely]] { \
            (scope__).clearException(); \
            return value__; \
        } \
    } while (false)

} // namespace JSC
