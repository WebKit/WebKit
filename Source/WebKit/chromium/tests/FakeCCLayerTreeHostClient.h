/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#ifndef FakeCCLayerTreeHostClient_h
#define FakeCCLayerTreeHostClient_h

#include "config.h"

#include "cc/CCLayerTreeHost.h"
#include "CompositorFakeGraphicsContext3D.h"

namespace WebCore {

class FakeCCLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    virtual void updateAnimations(double frameBeginTime) { }
    virtual void layout() { }
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) { }
    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D()
    {
        GraphicsContext3D::Attributes attrs;
        return createCompositorMockGraphicsContext3D(attrs);
    }
    virtual void didRecreateGraphicsContext(bool success) { }
    virtual void didCommitAndDrawFrame() { }
    virtual void didCompleteSwapBuffers() { }

    // Used only in the single-threaded path.
    virtual void scheduleComposite() { }
};

}
#endif // FakeCCLayerTreeHostClient_h
