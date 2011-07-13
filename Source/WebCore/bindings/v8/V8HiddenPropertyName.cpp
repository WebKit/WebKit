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

#include "config.h"
#include "V8HiddenPropertyName.h"

#include <string.h>
#include <wtf/Vector.h>

namespace WebCore {

#define V8_AS_STRING(x) V8_AS_STRING_IMPL(x)
#define V8_AS_STRING_IMPL(x) #x

#define V8_DEFINE_PROPERTY(name) \
v8::Handle<v8::String> V8HiddenPropertyName::name() \
{ \
    static v8::Persistent<v8::String>* string = createString("WebCore::HiddenProperty::" V8_AS_STRING(name)); \
    return *string; \
}

V8_HIDDEN_PROPERTIES(V8_DEFINE_PROPERTY);

static const char hiddenReferenceNamePrefix[] = "WebCore::HiddenReference::";

v8::Handle<v8::String> V8HiddenPropertyName::hiddenReferenceName(const char* name)
{
    Vector<char, 64> prefixedName;
    prefixedName.append(hiddenReferenceNamePrefix, sizeof(hiddenReferenceNamePrefix) - 1);
    prefixedName.append(name, strlen(name));
    return v8::String::NewSymbol(prefixedName.data(), static_cast<int>(prefixedName.size()));
}

v8::Persistent<v8::String>* V8HiddenPropertyName::createString(const char* key)
{
    v8::HandleScope scope;
    return new v8::Persistent<v8::String>(v8::Persistent<v8::String>::New(v8::String::NewSymbol(key)));
}

}  // namespace WebCore
