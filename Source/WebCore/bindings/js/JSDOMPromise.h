/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JSDOMPromise_h
#define JSDOMPromise_h

#include "JSCryptoKey.h"
#include "JSCryptoKeyPair.h"
#include "JSDOMBinding.h"
#include <heap/StrongInlines.h>
#include <runtime/JSPromiseDeferred.h>

namespace WebCore {

class DeferredWrapper {
public:
    DeferredWrapper(JSC::ExecState*, JSDOMGlobalObject*);

    template<class ResolveResultType>
    void resolve(const ResolveResultType&);

    template<class RejectResultType>
    void reject(const RejectResultType&);

    JSC::JSObject* promise() const;

private:
    void resolve(JSC::ExecState*, JSC::JSValue);
    void reject(JSC::ExecState*, JSC::JSValue);

    JSC::Strong<JSDOMGlobalObject> m_globalObject;
    JSC::Strong<JSC::JSPromiseDeferred> m_deferred;
};

template<class ResolveResultType>
inline void DeferredWrapper::resolve(const ResolveResultType& result)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(exec, toJS(exec, m_globalObject.get(), result));
}

template<class RejectResultType>
inline void DeferredWrapper::reject(const RejectResultType& result)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(exec, toJS(exec, m_globalObject.get(), result));
}

template<>
inline void DeferredWrapper::reject(const std::nullptr_t&)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(exec, JSC::jsNull());
}

template<>
inline void DeferredWrapper::resolve<String>(const String& result)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(exec, jsString(exec, result));
}

template<>
inline void DeferredWrapper::resolve<bool>(const bool& result)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(exec, JSC::jsBoolean(result));
}

template<>
inline void DeferredWrapper::resolve<Vector<unsigned char>>(const Vector<unsigned char>& result)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(result.data(), result.size());
    resolve(exec, toJS(exec, m_globalObject.get(), buffer.get()));
}

template<>
inline void DeferredWrapper::reject<String>(const String& result)
{
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(exec, jsString(exec, result));
}

}

#endif // JSDOMPromise_h
