/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#ifndef NPV8Object_h
#define NPV8Object_h

#include "V8DOMWrapper.h"

#if PLATFORM(CHROMIUM)
// FIXME: Chromium uses a different npruntime.h, which is in
// the Chromium source repository under third_party/npapi/bindings.
// The Google-specific changes in that file should probably be
// moved into bridge/npruntime.h, guarded by an #if PlATFORM(CHROMIUM).
#include <bindings/npruntime.h>
#else
#include "npruntime.h" // Use WebCore version for other ports.
#endif

#include <v8.h>

namespace WebCore {

class DOMWindow;

static const int npObjectInternalFieldCount = v8DefaultWrapperInternalFieldCount + 0;

WrapperTypeInfo* npObjectTypeInfo();

extern NPClass* npScriptObjectClass;

// A V8NPObject is a NPObject which carries additional V8-specific information. It is allocated and deallocated by
// AllocV8NPObject() and FreeV8NPObject() methods.
struct V8NPObject {
    NPObject object;
    v8::Persistent<v8::Object> v8Object;
    DOMWindow* rootObject;
};

struct PrivateIdentifier {
    union {
        const NPUTF8* string;
        int32_t number;
    } value;
    bool isString;
};

NPObject* npCreateV8ScriptObject(NPP, v8::Handle<v8::Object>, DOMWindow*);

NPObject* v8ObjectToNPObject(v8::Handle<v8::Object>);

} // namespace WebCore

#endif // NPV8Object_h
