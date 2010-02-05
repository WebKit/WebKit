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

#ifndef ScriptValue_h
#define ScriptValue_h

#include "PlatformString.h"
#include "ScriptState.h"
#include <runtime/Protect.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class String;
class SerializedScriptValue;

class ScriptValue {
public:
    ScriptValue(JSC::JSValue value = JSC::JSValue()) : m_value(value) {}
    virtual ~ScriptValue() {}

    JSC::JSValue jsValue() const { return m_value.get(); }
    bool getString(ScriptState*, String& result) const;
    String toString(ScriptState* scriptState) const { return m_value.get().toString(scriptState); }
    bool isEqual(ScriptState*, const ScriptValue&) const;
    bool isNull() const;
    bool isUndefined() const;
    bool isObject() const;
    bool hasNoValue() const { return m_value == JSC::JSValue(); }

    PassRefPtr<SerializedScriptValue> serialize(ScriptState*);
    static ScriptValue deserialize(ScriptState*, SerializedScriptValue*);

private:
    JSC::ProtectedJSValue m_value;
};

} // namespace WebCore

#endif // ScriptValue_h
