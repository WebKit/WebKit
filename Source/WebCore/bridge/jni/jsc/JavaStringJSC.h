/*
 * Copyright (C) 2003, 2004, 2005, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JavaStringJSC_h
#define JavaStringJSC_h

#include "JNIUtility.h"
#include "JavaInstanceJSC.h"

#include <runtime/JSLock.h>
#include <runtime/ScopeChain.h>


namespace JSC {

namespace Bindings {

class JavaString {
public:
    JavaString(JNIEnv* e, jstring s)
    {
        init(e, s);
    }

    JavaString(jstring s)
    {
        init(getJNIEnv(), s);
    }

    JavaString()
    {
        JSLock lock(SilenceAssertionsOnly);
        m_impl = UString().impl();
    }

    ~JavaString()
    {
        JSLock lock(SilenceAssertionsOnly);
        m_impl = 0;
    }

    const char* utf8() const
    {
        if (!m_utf8String.data()) {
            JSLock lock(SilenceAssertionsOnly);
            m_utf8String = UString(m_impl).utf8();
        }
        return m_utf8String.data();
    }
    int length() const { return m_impl->length(); }
    StringImpl* impl() const { return m_impl.get(); }

private:
    void init(JNIEnv* e, jstring s)
    {
        int size = e->GetStringLength(s);
        const jchar* uc = getUCharactersFromJStringInEnv(e, s);
        {
            JSLock lock(SilenceAssertionsOnly);
            m_impl = UString(reinterpret_cast<const UChar*>(uc), size).impl();
        }
        releaseUCharactersForJStringInEnv(e, s, uc);
    }

    RefPtr<StringImpl> m_impl;
    mutable CString m_utf8String;
};

} // namespace Bindings

} // namespace JSC

#endif // JavaStringJSC_h
