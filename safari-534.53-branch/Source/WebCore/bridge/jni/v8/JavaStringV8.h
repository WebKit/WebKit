/*
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

#ifndef JavaStringV8_h
#define JavaStringV8_h

#include "JNIUtility.h"

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>


namespace JSC {

namespace Bindings {

class JavaStringImpl {
public:
    void init() {}

    void init(JNIEnv* env, jstring string)
    {
        int size = env->GetStringLength(string);
        const jchar* jChars = getUCharactersFromJStringInEnv(env, string);
        m_impl = StringImpl::create(jChars, size);
        releaseUCharactersForJStringInEnv(env, string, jChars);
    }

    const char* utf8() const
    {
        if (!m_utf8String.data())
            m_utf8String = String(m_impl).utf8();
        return m_utf8String.data();
    }
    int length() const { return m_utf8String.length(); }
    StringImpl* impl() const { return m_impl.get(); }

private:
    RefPtr<StringImpl> m_impl;
    mutable CString m_utf8String;
};

} // namespace Bindings

} // namespace JSC

#endif // JavaStringV8_h
