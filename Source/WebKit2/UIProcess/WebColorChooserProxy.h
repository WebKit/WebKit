/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebColorChooserProxy_h
#define WebColorChooserProxy_h

#if ENABLE(INPUT_TYPE_COLOR)

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class Color;
}

namespace WebKit {

class WebPageProxy;

class WebColorChooserProxy : public RefCounted<WebColorChooserProxy> {
public:
    class Client {
    protected:
        virtual ~Client() { }

    public:
        virtual void didChooseColor(const WebCore::Color&) = 0;
        virtual void didEndColorChooser() = 0;
    };
    virtual ~WebColorChooserProxy() { }

    void invalidate() { m_client = 0; }

    virtual void endChooser() = 0;
    virtual void setSelectedColor(const WebCore::Color&) = 0;

protected:
    WebColorChooserProxy(Client* client)
        : m_client(client)
    {
    }

    Client* m_client;
};

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)

#endif // WebColorChooserProxy_h
