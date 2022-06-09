/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef WebContextSupplement_h
#define WebContextSupplement_h

namespace WebKit {

class NetworkProcessProxy;
class WebProcessPool;
class WebProcessProxy;

class WebContextSupplement {
public:
    WebContextSupplement(WebProcessPool* processPool)
        : m_processPool(processPool)
    {
    }

    virtual ~WebContextSupplement()
    {
    }

    virtual void processPoolDestroyed()
    {
    }

    virtual void processDidClose(WebProcessProxy*)
    {
    }

    virtual void processDidClose(NetworkProcessProxy*)
    {
    }

    WebProcessPool* processPool() const { return m_processPool; }
    void clearProcessPool() { m_processPool = nullptr; }

    void ref() { refWebContextSupplement(); }
    void deref() { derefWebContextSupplement(); }

private:
    virtual void refWebContextSupplement() = 0;
    virtual void derefWebContextSupplement() = 0;

    WebProcessPool* m_processPool;
};

} // namespace WebKit

#endif // WebContextSupplement_h
