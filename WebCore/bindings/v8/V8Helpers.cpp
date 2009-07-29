/*
* Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "V8Helpers.h"

#include "DOMWindow.h"
#include "NPV8Object.h"
#include "V8Index.h"
#include "V8Proxy.h"

namespace WebCore {

void wrapNPObject(v8::Handle<v8::Object> object, NPObject* npObject)
{
    V8DOMWrapper::setDOMWrapper(object, V8ClassIndex::NPOBJECT, npObject);
}

v8::Local<v8::Context> toV8Context(NPP npp, NPObject* npObject)
{
    V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);
    return V8Proxy::mainWorldContext(object->rootObject->frame());
}

V8Proxy* toV8Proxy(NPObject* npObject)
{
    V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);
    Frame* frame = object->rootObject->frame();
    return V8Proxy::retrieve(frame);
}

} // namespace WebCore
