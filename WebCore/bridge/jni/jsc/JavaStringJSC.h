/*
 * Copyright (C) 2010 Apple Computer, Inc.  All rights reserved.
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


namespace JSC {

namespace Bindings {

class JavaStringImpl {
public:
    ~JavaStringImpl()
    {
        JSLock lock(SilenceAssertionsOnly);
        m_rep = 0;
    }

    void init()
    {
        JSLock lock(SilenceAssertionsOnly);
        m_rep = UString().rep();
    }

    void init(JNIEnv* e, jstring s)
    {
        int size = e->GetStringLength(s);
        const jchar* uc = getUCharactersFromJStringInEnv(e, s);
        {
            JSLock lock(SilenceAssertionsOnly);
            m_rep = UString(reinterpret_cast<const UChar*>(uc), size).rep();
        }
        releaseUCharactersForJStringInEnv(e, s, uc);
    }

    const char* UTF8String() const
    {
        if (!m_utf8String.c_str()) {
            JSLock lock(SilenceAssertionsOnly);
            m_utf8String = UString(m_rep).UTF8String();
        }
        return m_utf8String.c_str();
    }
    const jchar* uchars() const { return (const jchar*)m_rep->data(); }
    int length() const { return m_rep->size(); }
    UString uString() const { return UString(m_rep); }

private:
    RefPtr<UString::Rep> m_rep;
    mutable CString m_utf8String;
};

} // namespace Bindings

} // namespace JSC

#endif // JavaStringJSC_h
