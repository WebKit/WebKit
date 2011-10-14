/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef NPObjectWrapper_h
#define NPObjectWrapper_h

#include "npruntime_impl.h"

namespace WebCore {

// This class wraps a NPObject and provides functionality for the wrapped
// object to be cleared out when this object is destroyed. This is to ensure
// that callers trying to access the underlying object don't crash while
// invoking methods on the NPObject.
class NPObjectWrapper {
public:
    // Creates an instance of the NPObjectWrapper class and wraps the object
    // passed in.
    static NPObject* create(NPObject* object);

    // This method should be called to invalidate the underlying NPObject pointer.
    void clear();

    // Returns a pointer to NPObjectWrapper if the object passed in was wrapped by us.
    static NPObjectWrapper* getWrapper(NPObject* obj);

    // Returns a pointer to the underlying raw NPObject pointer or 0 if the object
    // passed in was not wrapped.
    static NPObject* getUnderlyingNPObject(NPObject* obj);

    // NPObject functions implemented by the wrapper.
    static NPObject* NPAllocate(NPP, NPClass*);
    static void NPDeallocate(NPObject* obj);
    static void NPPInvalidate(NPObject *obj);
    static bool NPHasMethod(NPObject* obj, NPIdentifier name);
    static bool NPInvoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
    static bool NPInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result);
    static bool NPHasProperty(NPObject* obj, NPIdentifier name);
    static bool NPGetProperty(NPObject* obj, NPIdentifier name, NPVariant* result);
    static bool NPSetProperty(NPObject* obj, NPIdentifier name, const NPVariant *value);
    static bool NPRemoveProperty(NPObject* obj, NPIdentifier name);
    static bool NPNEnumerate(NPObject* obj, NPIdentifier **value, uint32_t* count);
    static bool NPNConstruct(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result);
    static bool NPInvokePrivate(NPP npp, NPObject* obj,bool isDefault, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);

private:
    NPObjectWrapper(NPObject* obj);

    // Returns the underlying NPObject if the object passed in was wrapped. Otherwise
    // just returns the object passed in.
    static NPObject* getObjectForCall(NPObject* obj);

    static NPClass m_npClassWrapper;
    // Weak NPObject poointer.
    NPObject* m_wrappedNPObject;
};

} // namespace WebCore

#endif // NPObjectWrapper_h

