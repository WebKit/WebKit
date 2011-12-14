/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#ifndef AssociatedURLLoader_h
#define AssociatedURLLoader_h

#include "WebURLLoader.h"
#include "WebURLLoaderOptions.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore { class DocumentThreadableLoader; }

namespace WebKit {

class WebFrameImpl;

// This class is used to implement WebFrame::createAssociatedURLLoader.
class AssociatedURLLoader : public WebURLLoader {
    WTF_MAKE_NONCOPYABLE(AssociatedURLLoader);
public:
    AssociatedURLLoader(PassRefPtr<WebFrameImpl>);
    AssociatedURLLoader(PassRefPtr<WebFrameImpl>, const WebURLLoaderOptions&);
    ~AssociatedURLLoader();

    // WebURLLoader methods:
    virtual void loadSynchronously(const WebURLRequest&, WebURLResponse&, WebURLError&, WebData&);
    virtual void loadAsynchronously(const WebURLRequest&, WebURLLoaderClient*);
    virtual void cancel();
    virtual void setDefersLoading(bool);

private:

    class ClientAdapter;

    RefPtr<WebFrameImpl> m_frameImpl;
    WebURLLoaderOptions m_options;
    WebURLLoaderClient* m_client;
    OwnPtr<ClientAdapter> m_clientAdapter;
    RefPtr<WebCore::DocumentThreadableLoader> m_loader;
};

} // namespace WebKit

#endif
