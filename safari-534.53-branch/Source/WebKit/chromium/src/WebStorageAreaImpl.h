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

#ifndef WebStorageAreaImpl_h
#define WebStorageAreaImpl_h

#if ENABLE(DOM_STORAGE)

#include "StorageAreaImpl.h"
#include "WebStorageArea.h"

namespace WebKit {

class WebStorageAreaImpl : public WebStorageArea {
public:
    WebStorageAreaImpl(PassRefPtr<WebCore::StorageArea> storageArea);
    virtual ~WebStorageAreaImpl();
    virtual unsigned length();
    virtual WebString key(unsigned index);
    virtual WebString getItem(const WebString& key);
    virtual void setItem(const WebString& key, const WebString& value, const WebURL& url, Result& result, WebString& oldValue, WebFrame*);
    virtual void removeItem(const WebString& key, const WebURL& url, WebString& oldValue);
    virtual void clear(const WebURL& url, bool& somethingCleared);

    // For storage events in single-process mode and test shell.
    static const WebURL* currentStorageEventURL() { return storageEventURL; }

private:
    class ScopedStorageEventURL {
    public:
        ScopedStorageEventURL(const WebURL& url)
        {
            // FIXME: Once storage events are fired async in WebKit (as they should
            //        be) this can be ASSERTed to be 0 rather than saved.
            m_existingStorageEventURL = storageEventURL;
            storageEventURL = &url;
        }
        ~ScopedStorageEventURL()
        {
            storageEventURL = m_existingStorageEventURL;
        }

    private:
        const WebURL* m_existingStorageEventURL;
    };

    static const WebURL* storageEventURL;

    RefPtr<WebCore::StorageArea> m_storageArea;
};

} // namespace WebKit

#endif // ENABLE(DOM_STORAGE)

#endif // WebStorageAreaImpl_h
