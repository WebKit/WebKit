/*
* Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V8ObjectConstructor_h
#define V8ObjectConstructor_h

#include "V8PerIsolateData.h"

#include <v8.h>

namespace WebCore {

class Document;

class ConstructorMode {
public:
    enum Mode {
        WrapExistingObject,
        CreateNewObject
    };

    ConstructorMode()
    {
        V8PerIsolateData* data = V8PerIsolateData::current();
        m_previous = data->m_constructorMode;
        data->m_constructorMode = WrapExistingObject;
    }

    ~ConstructorMode()
    {
        V8PerIsolateData* data = V8PerIsolateData::current();
        data->m_constructorMode = m_previous;
    }

    static bool current() { return V8PerIsolateData::current()->m_constructorMode; }

private:
    bool m_previous;
};

class V8ObjectConstructor {
public:
    static v8::Local<v8::Object> newInstance(v8::Handle<v8::Function>);
    static v8::Local<v8::Object> newInstance(v8::Handle<v8::ObjectTemplate>);
    static v8::Local<v8::Object> newInstance(v8::Handle<v8::Function>, int, v8::Handle<v8::Value> argv[]);
    static v8::Local<v8::Object> newInstanceInDocument(v8::Handle<v8::Function>, int, v8::Handle<v8::Value> argv[], Document*);

    static v8::Handle<v8::Value> isValidConstructorMode(const v8::Arguments&);
};

} // namespace WebCore

#endif // V8ObjectConstructor_h
