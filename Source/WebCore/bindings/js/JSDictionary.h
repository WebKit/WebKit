/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef JSDictionary_h
#define JSDictionary_h

#include "MessagePort.h"
#include <interpreter/CallFrame.h>
#include <wtf/Forward.h>

namespace WebCore {

class DOMWindow;
class EventTarget;
class Node;
class ScriptValue;
class SerializedScriptValue;
class Storage;
class TrackBase;

class JSDictionary {
public:
    JSDictionary(JSC::ExecState* exec, JSC::JSObject* initializerObject)
        : m_exec(exec)
        , m_initializerObject(initializerObject)
    {
    }

    template <typename Result>
    bool tryGetProperty(const char* propertyName, Result&);

    template <typename T, typename Result>
    bool tryGetProperty(const char* propertyName, T* context, void (*setter)(T* context, const Result&));

private:
    template <typename Result>
    struct IdentitySetter {
        static void identitySetter(Result* context, const Result& result)
        {
            *context = result;
        }
    };

    enum GetPropertyResult {
        ExceptionThrown,
        NoPropertyFound,
        PropertyFound
    };
    GetPropertyResult tryGetProperty(const char* propertyName, JSC::JSValue&);

    static void convertValue(JSC::ExecState*, JSC::JSValue, bool& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, int& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, unsigned& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, unsigned short& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, unsigned long long& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, double& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, String& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, ScriptValue& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, RefPtr<SerializedScriptValue>& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, RefPtr<DOMWindow>& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, RefPtr<EventTarget>& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, RefPtr<Node>& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, RefPtr<Storage>& result);
    static void convertValue(JSC::ExecState*, JSC::JSValue, MessagePortArray& result);
#if ENABLE(VIDEO_TRACK)
    static void convertValue(JSC::ExecState*, JSC::JSValue, RefPtr<TrackBase>& result);
#endif

    JSC::ExecState* m_exec;
    JSC::JSObject* m_initializerObject;
};


template <typename T, typename Result>
bool JSDictionary::tryGetProperty(const char* propertyName, T* context, void (*setter)(T* context, const Result&))
{
    JSC::JSValue value;
    switch (tryGetProperty(propertyName, value)) {
    case ExceptionThrown:
        return false;
    case PropertyFound: {
        Result result;
        convertValue(m_exec, value, result);

        if (m_exec->hadException())
            return false;

        setter(context, result);
        break;
    }
    case NoPropertyFound:
        break;
    }

    return true;
}

template <typename Result>
bool JSDictionary::tryGetProperty(const char* propertyName, Result& finalResult)
{
    return tryGetProperty(propertyName, &finalResult, IdentitySetter<Result>::identitySetter);
}

} // namespace WebCore

#endif // JSDictionary_h
