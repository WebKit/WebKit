/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#ifndef ScriptString_h
#define ScriptString_h

#include "JSDOMBinding.h"
#include "PlatformString.h"
#include <runtime/UString.h>
#include <runtime/StringBuilder.h>

namespace WebCore {

class String;

class ScriptString {
public:
    ScriptString() {}
    ScriptString(const char* s) : m_str(s) {}
    ScriptString(const String& s) : m_str(stringToUString(s)) {}
    ScriptString(const JSC::UString& s) : m_str(s) {}

    operator JSC::UString() const { return m_str; }
    operator String() const { return ustringToString(m_str); }
    const JSC::UString& ustring() const { return m_str; }

    bool isNull() const { return m_str.isNull(); }
    size_t size() const { return m_str.size(); }

    ScriptString& operator=(const char* s)
    {
        m_str = s;
        return *this;
    }

    ScriptString& operator+=(const String& s)
    {
        JSC::StringBuilder buffer;
        buffer.append(m_str);
        buffer.append(stringToUString(s));
        m_str = buffer.build();
        return *this;
    }

    bool operator==(const ScriptString& s) const
    {
        return m_str == s.m_str;
    }

    bool operator!=(const ScriptString& s) const
    {
        // Avoid exporting an extra symbol by re-using "==" operator.
        return !(m_str == s.m_str);
    }

private:
    JSC::UString m_str;
};

} // namespace WebCore

#endif // ScriptString_h
