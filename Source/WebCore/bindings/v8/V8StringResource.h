/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8StringResource_h
#define V8StringResource_h

#include <v8.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum ExternalMode {
    Externalize,
    DoNotExternalize
};

template <typename StringType>
StringType v8StringToWebCoreString(v8::Handle<v8::String>, ExternalMode);
String int32ToWebCoreString(int value);

// V8Parameter is an adapter class that converts V8 values to Strings
// or AtomicStrings as appropriate, using multiple typecast operators.
enum V8ParameterMode {
    DefaultMode,
    WithNullCheck,
    WithUndefinedOrNullCheck
};

template <V8ParameterMode Mode = DefaultMode>
class V8Parameter {
public:
    V8Parameter(v8::Local<v8::Value> object)
        : m_v8Object(object)
        , m_mode(Externalize)
        , m_string()
    {
    }

    // This can throw. You must use this through the
    // STRING_TO_V8PARAMETER_EXCEPTION_BLOCK() macro.
    void prepare();
    operator String() { return toString<String>(); }
    operator AtomicString() { return toString<AtomicString>(); }

private:
    void prepareBase()
    {
        if (m_v8Object.IsEmpty())
            return;

        if (LIKELY(m_v8Object->IsString()))
            return;

        if (LIKELY(m_v8Object->IsInt32())) {
            setString(int32ToWebCoreString(m_v8Object->Int32Value()));
            return;
        }

        m_mode = DoNotExternalize;
        m_v8Object = m_v8Object->ToString();
    }

    v8::Local<v8::Value> object() { return m_v8Object; }

    void setString(const String& string)
    {
        m_string = string;
        m_v8Object.Clear(); // To signal that String is ready.
    }

    template <class StringType>
    StringType toString()
    {
        if (LIKELY(!m_v8Object.IsEmpty()))
            return v8StringToWebCoreString<StringType>(m_v8Object.As<v8::String>(), m_mode);

        return StringType(m_string);
    }

    v8::Local<v8::Value> m_v8Object;
    ExternalMode m_mode;
    String m_string;
};

template<> inline void V8Parameter<DefaultMode>::prepare()
{
    prepareBase();
}

template<> inline void V8Parameter<WithNullCheck>::prepare()
{
    if (object().IsEmpty() || object()->IsNull()) {
        setString(String());
        return;
    }
    prepareBase();
}

template<> inline void V8Parameter<WithUndefinedOrNullCheck>::prepare()
{
    if (object().IsEmpty() || object()->IsNull() || object()->IsUndefined()) {
        setString(String());
        return;
    }
    prepareBase();
}
    
} // namespace WebCore

#endif // V8StringResource_h
