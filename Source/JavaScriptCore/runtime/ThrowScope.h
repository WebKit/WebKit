/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef ThrowScope_h
#define ThrowScope_h

#include "Exception.h"
#include "VM.h"

namespace JSC {

class ExecState;
class JSObject;

#if ENABLE(THROW_SCOPE_VERIFICATION)

class ThrowScope {
public:
    JS_EXPORT_PRIVATE ThrowScope(VM&, ThrowScopeLocation);
    JS_EXPORT_PRIVATE ~ThrowScope();

    ThrowScope(const ThrowScope&) = delete;
    ThrowScope(ThrowScope&&) = default;

    VM& vm() const { return m_vm; }

    JS_EXPORT_PRIVATE void throwException(ExecState*, Exception*);
    JS_EXPORT_PRIVATE JSValue throwException(ExecState*, JSValue);
    JS_EXPORT_PRIVATE JSObject* throwException(ExecState*, JSObject*);

    inline Exception* exception()
    {
        m_vm.m_needExceptionCheck = false;
        return m_vm.exception();
    }

    inline void release() { m_isReleased = true; }

    JS_EXPORT_PRIVATE void printIfNeedCheck(const char* functionName, const char* file, unsigned line);

private:
    void simulateThrow();

    enum class Site {
        ScopeEntry,
        ScopeExit,
        Throw
    };
    void verifyExceptionCheckNeedIsSatisfied(Site);

    VM& m_vm;
    ThrowScope* m_previousScope;
    ThrowScopeLocation m_location;
    unsigned m_depth;
    bool m_isReleased { false };
};

#define DECLARE_THROW_SCOPE(vm__) \
    JSC::ThrowScope((vm__), JSC::ThrowScopeLocation(__FUNCTION__, __FILE__, __LINE__))

#define throwScopePrintIfNeedCheck(scope__) \
    scope__.printIfNeedCheck(__FUNCTION__, __FILE__, __LINE__)

#else // not ENABLE(THROW_SCOPE_VERIFICATION)

class ThrowScope {
public:
    ThrowScope(VM& vm)
        : m_vm(vm)
    { }

    VM& vm() const { return m_vm; }
    
    void throwException(ExecState* exec, Exception* exception) { m_vm.throwException(exec, exception); }
    JSValue throwException(ExecState* exec, JSValue value) { return m_vm.throwException(exec, value); }
    JSObject* throwException(ExecState* exec, JSObject* obj) { return m_vm.throwException(exec, obj); }
    
    Exception* exception() { return m_vm.exception(); }
    void release() { }
    
private:
    VM& m_vm;
};

#define DECLARE_THROW_SCOPE(vm__) \
    JSC::ThrowScope((vm__))

#endif // ENABLE(THROW_SCOPE_VERIFICATION)

ALWAYS_INLINE void throwException(ExecState* exec, ThrowScope& scope, Exception* exception)
{
    scope.throwException(exec, exception);
}

ALWAYS_INLINE JSValue throwException(ExecState* exec, ThrowScope& scope, JSValue value)
{
    return scope.throwException(exec, value);
}

ALWAYS_INLINE JSObject* throwException(ExecState* exec, ThrowScope& scope, JSObject* obj)
{
    return scope.throwException(exec, obj);
}

} // namespace JSC

#endif // ThrowScope_h
