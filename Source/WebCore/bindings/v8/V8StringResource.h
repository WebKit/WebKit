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

class V8ParameterBase {
public:
    operator String() { return toString<String>(); }
    operator AtomicString() { return toString<AtomicString>(); }

protected:
    V8ParameterBase(v8::Local<v8::Value> object) : m_v8Object(object), m_mode(Externalize), m_string() { }

    bool prepareBase()
    {
        if (m_v8Object.IsEmpty())
            return true;

        if (LIKELY(m_v8Object->IsString()))
            return true;

        if (LIKELY(m_v8Object->IsInt32())) {
            setString(int32ToWebCoreString(m_v8Object->Int32Value()));
            return true;
        }

        m_mode = DoNotExternalize;
        v8::TryCatch block;
        m_v8Object = m_v8Object->ToString();
        // Handle the case where an exception is thrown as part of invoking toString on the object.
        if (block.HasCaught()) {
            block.ReThrow();
            return false;
        }
        return true;
    }

    v8::Local<v8::Value> object() { return m_v8Object; }

    void setString(const String& string)
    {
        m_string = string;
        m_v8Object.Clear(); // To signal that String is ready.
    }

private:
    v8::Local<v8::Value> m_v8Object;
    ExternalMode m_mode;
    String m_string;

    template <class StringType>
    StringType toString()
    {
        if (LIKELY(!m_v8Object.IsEmpty()))
            return v8StringToWebCoreString<StringType>(m_v8Object.As<v8::String>(), m_mode);

        return StringType(m_string);
    }
};

// V8Parameter is an adapter class that converts V8 values to Strings
// or AtomicStrings as appropriate, using multiple typecast operators.
enum V8ParameterMode {
    DefaultMode,
    WithNullCheck,
    WithUndefinedOrNullCheck
};

template <V8ParameterMode MODE = DefaultMode>
class V8Parameter: public V8ParameterBase {
public:
    V8Parameter(v8::Local<v8::Value> object) : V8ParameterBase(object) { }
    V8Parameter(v8::Local<v8::Value> object, bool) : V8ParameterBase(object) { prepare(); }

    bool prepare();
};

template<> inline bool V8Parameter<DefaultMode>::prepare()
{
    return V8ParameterBase::prepareBase();
}

template<> inline bool V8Parameter<WithNullCheck>::prepare()
{
    if (object().IsEmpty() || object()->IsNull()) {
        setString(String());
        return true;
    }

    return V8ParameterBase::prepareBase();
}

template<> inline bool V8Parameter<WithUndefinedOrNullCheck>::prepare()
{
    if (object().IsEmpty() || object()->IsNull() || object()->IsUndefined()) {
        setString(String());
        return true;
    }

    return V8ParameterBase::prepareBase();
}
    
} // namespace WebCore

#endif // V8StringResource_h
