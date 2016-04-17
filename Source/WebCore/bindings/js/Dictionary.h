/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSDictionary.h"
#include "JSEventListener.h"
#include <bindings/ScriptValue.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSValue;
}

namespace WebCore {
class EventListener;

class Dictionary {
public:
    Dictionary();
    Dictionary(JSC::ExecState*, JSC::JSValue);

    // Returns true if a value was found for the provided property.
    template<typename Result> bool get(const char* propertyName, Result&) const;
    template<typename Result> bool get(const String& propertyName, Result&) const;

    template<typename Result> Optional<Result> get(const char* propertyName) const;

    template<typename T> RefPtr<EventListener> getEventListener(const char* propertyName, T* target) const;
    template<typename T> RefPtr<EventListener> getEventListener(const String& propertyName, T* target) const;

    bool isObject() const { return m_dictionary.isValid(); }
    bool isUndefinedOrNull() const { return !m_dictionary.isValid(); }
    bool getOwnPropertiesAsStringHashMap(HashMap<String, String>&) const;
    bool getOwnPropertyNames(Vector<String>&) const;
    bool getWithUndefinedOrNullCheck(const char* propertyName, String& value) const;

    JSC::ExecState* execState() const { return m_dictionary.execState(); }

private:
    template<typename T> JSC::JSObject* asJSObject(T*) const;
    
    JSDictionary m_dictionary;
};

template<typename Result> bool Dictionary::get(const char* propertyName, Result& result) const
{
    return m_dictionary.isValid() && m_dictionary.get(propertyName, result);
}

template<typename Result> bool Dictionary::get(const String& propertyName, Result& result) const
{
    return get(propertyName.utf8().data(), result);
}

template<typename Result> Optional<Result> Dictionary::get(const char* propertyName) const
{
    Result result;
    if (!get(propertyName, result))
        return Nullopt;
    return result;
}

template<typename T> RefPtr<EventListener> Dictionary::getEventListener(const char* propertyName, T* target) const
{
    JSC::JSValue eventListener;
    if (!get(propertyName, eventListener) || !eventListener || !eventListener.isObject())
        return nullptr;
    return JSEventListener::create(asObject(eventListener), asJSObject(target), true, currentWorld(execState()));
}

template<typename T> RefPtr<EventListener> Dictionary::getEventListener(const String& propertyName, T* target) const
{
    return getEventListener(propertyName.utf8().data(), target);
}

}
