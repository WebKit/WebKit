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


#ifndef V8CustomXPathNSResolver_h
#define V8CustomXPathNSResolver_h

#if ENABLE(XPATH)

#include "XPathNSResolver.h"
#include <v8.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class V8Proxy;

// V8CustomXPathNSResolver does not create a persistent handle to the
// given resolver object.  So the lifetime of V8CustomXPathNSResolver
// must not exceed the lifetime of the passed handle.
class V8CustomXPathNSResolver : public XPathNSResolver {
public:
    static PassRefPtr<V8CustomXPathNSResolver> create(v8::Handle<v8::Object> resolver);

    virtual ~V8CustomXPathNSResolver();
    virtual String lookupNamespaceURI(const String& prefix);

private:
    explicit V8CustomXPathNSResolver(v8::Handle<v8::Object> resolver);

    v8::Handle<v8::Object> m_resolver;  // Handle to resolver object.
};

} // namespace WebCore

#endif  // ENABLE(XPATH)

#endif  // V8CustomXPathNSResolver_h
