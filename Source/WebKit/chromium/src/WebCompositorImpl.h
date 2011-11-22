/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCompositorImpl_h
#define WebCompositorImpl_h

#include "WebCompositor.h"

#include "cc/CCInputHandler.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WTF {
class Mutex;
}

namespace WebKit {

class WebCompositorClient;

class WebCompositorImpl : public WebCompositor, public WebCore::CCInputHandler {
    WTF_MAKE_NONCOPYABLE(WebCompositorImpl);
public:
    static WebCompositor* fromIdentifier(int identifier);

    static PassOwnPtr<WebCompositorImpl> create(WebCore::CCInputHandlerClient* client)
    {
        return adoptPtr(new WebCompositorImpl(client));
    }

    virtual ~WebCompositorImpl();

    // WebCompositor implementation
    virtual void setClient(WebCompositorClient*);
    virtual void handleInputEvent(const WebInputEvent&);

    // WebCore::CCInputHandler implementation
    virtual int identifier() const;
    virtual void willDraw(double frameBeginTime);

private:
    explicit WebCompositorImpl(WebCore::CCInputHandlerClient*);

    WebCompositorClient* m_client;
    int m_identifier;
    WebCore::CCInputHandlerClient* m_inputHandlerClient;

    static int s_nextAvailableIdentifier;
    static HashSet<WebCompositorImpl*>* s_compositors;
};

}

#endif // WebCompositorImpl_h
