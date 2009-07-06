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

#ifndef V8Binding_h
#define V8Binding_h

// FIXME: This is a temporary forwarding header until all bindings have migrated
// over and v8_binding actually becomes V8Binding.
#include "v8_binding.h"

namespace WebCore {

    // FIXME: Remove once migration is complete.
    inline int toInt32(v8::Handle<v8::Value> value)
    {
        return ToInt32(value);
    }

    inline float toFloat(v8::Local<v8::Value> value)
    {
        return static_cast<float>(value->NumberValue());
    }

    // FIXME: Remove once migration is complete.
    inline String toWebCoreString(v8::Handle<v8::Value> obj)
    {
        return ToWebCoreString(obj);
    }

    // FIXME: Remove once migration is complete.
    inline const uint16_t* fromWebCoreString(const String& str)
    {
        return FromWebCoreString(str);
    }

    // FIXME: Rename valueToStringWithNullCheck once migration is complete.
    inline String toWebCoreStringWithNullCheck(v8::Handle<v8::Value> value)
    {
        return valueToStringWithNullCheck(value);
    }

    inline bool isUndefinedOrNull(v8::Handle<v8::Value> value)
    {
        return value->IsNull() || value->IsUndefined();
    }

    inline v8::Handle<v8::Boolean> v8Boolean(bool value)
    {
        return value ? v8::True() : v8::False();
    }

}

#endif // V8Binding_h
