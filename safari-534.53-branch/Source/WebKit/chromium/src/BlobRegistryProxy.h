/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef BlobRegistryProxy_h
#define BlobRegistryProxy_h

#if ENABLE(BLOB)

#include "BlobRegistry.h"

namespace WebKit { class WebBlobRegistry; }

namespace WebCore {

class BlobRegistryProxy : public BlobRegistry {
public:
    BlobRegistryProxy();

    virtual void registerBlobURL(const KURL&, PassOwnPtr<BlobData>);
    virtual void registerBlobURL(const KURL&, const KURL& srcURL);
    virtual void unregisterBlobURL(const KURL&);

    virtual PassRefPtr<ResourceHandle> createResourceHandle(const ResourceRequest&, ResourceHandleClient*) { return 0; }
    virtual bool loadResourceSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<char>& data) { return false; }

private:
    virtual ~BlobRegistryProxy() { }

    WebKit::WebBlobRegistry* m_webBlobRegistry;
};

} // namespace WebCore

#endif // ENABLE(BLOB)

#endif // BlobRegistryProxy_h
