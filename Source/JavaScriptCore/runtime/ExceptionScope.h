/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "VM.h"

namespace JSC {
    
class Exception;
    
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)

#define EXCEPTION_ASSERT(assertion) RELEASE_ASSERT(assertion)
#define EXCEPTION_ASSERT_UNUSED(variable, assertion) RELEASE_ASSERT(assertion)
#define EXCEPTION_ASSERT_WITH_MESSAGE(assertion, message) RELEASE_ASSERT_WITH_MESSAGE(assertion, message)

class ExceptionScope {
public:
    VM& vm() const { return m_vm; }
    unsigned recursionDepth() const { return m_recursionDepth; }
    Exception* exception() { return m_vm.exception(); }

    ALWAYS_INLINE void assertNoException() { RELEASE_ASSERT_WITH_MESSAGE(!exception(), "%s", unexpectedExceptionMessage().data()); }
    ALWAYS_INLINE void releaseAssertNoException() { RELEASE_ASSERT_WITH_MESSAGE(!exception(), "%s", unexpectedExceptionMessage().data()); }

protected:
    ExceptionScope(VM&, ExceptionEventLocation);
    ExceptionScope(const ExceptionScope&) = delete;
    ExceptionScope(ExceptionScope&&) = default;
    ~ExceptionScope();

    JS_EXPORT_PRIVATE CString unexpectedExceptionMessage();

    VM& m_vm;
    ExceptionScope* m_previousScope;
    ExceptionEventLocation m_location;
    unsigned m_recursionDepth;
};

#else // not ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    
#define EXCEPTION_ASSERT(x) ASSERT(x)
#define EXCEPTION_ASSERT_UNUSED(variable, assertion) ASSERT_UNUSED(variable, assertion)
#define EXCEPTION_ASSERT_WITH_MESSAGE(assertion, message) ASSERT_WITH_MESSAGE(assertion, message)

class ExceptionScope {
public:
    ALWAYS_INLINE VM& vm() const { return m_vm; }
    ALWAYS_INLINE Exception* exception() { return m_vm.exception(); }

    ALWAYS_INLINE void assertNoException() { ASSERT(!exception()); }
    ALWAYS_INLINE void releaseAssertNoException() { RELEASE_ASSERT(!exception()); }

protected:
    ALWAYS_INLINE ExceptionScope(VM& vm)
        : m_vm(vm)
    { }
    ExceptionScope(const ExceptionScope&) = delete;
    ExceptionScope(ExceptionScope&&) = default;

    ALWAYS_INLINE CString unexpectedExceptionMessage() { return { }; }

    VM& m_vm;
};

#endif // ENABLE(EXCEPTION_SCOPE_VERIFICATION)

#define RETURN_IF_EXCEPTION(scope__, value__) do { \
        if (UNLIKELY((scope__).exception())) \
            return value__; \
    } while (false)

#define RELEASE_AND_RETURN(scope__, expression__) do { \
        scope__.release(); \
        return expression__; \
    } while (false)

} // namespace JSC
