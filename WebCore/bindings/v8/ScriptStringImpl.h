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

#ifndef ScriptStringImpl_h
#define ScriptStringImpl_h

#include "OwnHandle.h"
#include "PlatformString.h"

#include <v8.h>

namespace WebCore {

// This class is used for strings that tend to be shared with JavaScript frequently.  The JSC implementation uses wtf::UString - see bindings/js/ScriptString.h
// Currently XMLHttpRequest uses a ScriptString to build up the responseText attribute.  As data arrives from the network, it is appended to the ScriptString
// via operator+= and a JavaScript readystatechange event is fired.  JavaScript can access the responseText attribute of the XMLHttpRequest object.  JavaScript
// may also query the responseXML attribute of the XMLHttpRequest object which results in the responseText attribute being coerced into a WebCore::String and
// then parsed as an XML document.
// This implementation optimizes for the common case where the responseText is built up with many calls to operator+= before the actual text is queried.
class ScriptStringImpl : public RefCounted<ScriptStringImpl> {
public:
    ScriptStringImpl() {}
    ScriptStringImpl(const String& s);
    ScriptStringImpl(const char* s);

    String toString() const;

    bool isNull() const;
    size_t size() const;

    void append(const String& s);

    v8::Handle<v8::String> v8StringHandle() { return m_handle.get(); }

private:
    OwnHandle<v8::String> m_handle;
};

} // namespace WebCore

#endif // ScriptStringImpl_h
