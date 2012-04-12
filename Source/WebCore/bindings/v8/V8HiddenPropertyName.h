/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8HiddenPropertyName_h
#define V8HiddenPropertyName_h

#include <v8.h>

namespace WebCore {

#define V8_HIDDEN_PROPERTIES(V) \
    V(objectPrototype) \
    V(listener) \
    V(attributeListener) \
    V(scriptState) \
    V(devtoolsInjectedScript) \
    V(sleepFunction) \
    V(toStringString) \
    V(event) \
    V(state) \
    V(domStringMap) \
    V(domTokenList) \
    V(ownerNode)

    class V8HiddenPropertyName {
    public:
        V8HiddenPropertyName() { }
#define V8_DECLARE_PROPERTY(name) static v8::Handle<v8::String> name();
        V8_HIDDEN_PROPERTIES(V8_DECLARE_PROPERTY);
#undef V8_DECLARE_PROPERTY

        static v8::Handle<v8::String> hiddenReferenceName(const char* name);

    private:
        static v8::Persistent<v8::String> createString(const char* key);
#define V8_DECLARE_FIELD(name) v8::Persistent<v8::String> m_##name;
        V8_HIDDEN_PROPERTIES(V8_DECLARE_FIELD);
#undef V8_DECLARE_FIELD
    };

}

#endif // V8HiddenPropertyName_h
