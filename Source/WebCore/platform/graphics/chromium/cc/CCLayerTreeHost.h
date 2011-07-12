/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerTreeHost_h
#define CCLayerTreeHost_h

#include "cc/CCLayerTreeHostCommitter.h"
#include "cc/CCLayerTreeHostImplProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CCLayerTreeHostImpl;
class CCLayerTreeHostImplClient;
class GraphicsContext3D;

class CCLayerTreeHostClient {
public:
    virtual void animateAndLayout(double frameBeginTime) = 0;
    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D() = 0;

protected:
    virtual ~CCLayerTreeHostClient() { }
};

class CCLayerTreeHost : public RefCounted<CCLayerTreeHost> {
public:
    explicit CCLayerTreeHost(CCLayerTreeHostClient*);
    void init();
    virtual ~CCLayerTreeHost();

    virtual void animateAndLayout(double frameBeginTime);
    virtual void beginCommit();
    virtual void commitComplete();
    virtual PassOwnPtr<CCLayerTreeHostCommitter> createLayerTreeHostCommitter();

    int frameNumber() const { return m_frameNumber; }

    void setNeedsCommitAndRedraw();
    void setNeedsRedraw();

protected:
    virtual PassOwnPtr<CCLayerTreeHostImplProxy> createLayerTreeHostImplProxy() = 0;

private:
    CCLayerTreeHostClient* m_client;
    int m_frameNumber;
    OwnPtr<CCLayerTreeHostImplProxy> m_proxy;
};

}

#endif
