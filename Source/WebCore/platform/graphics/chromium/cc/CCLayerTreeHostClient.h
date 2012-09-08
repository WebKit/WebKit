/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef CCLayerTreeHostClient_h
#define CCLayerTreeHostClient_h

#include <wtf/PassOwnPtr.h>

namespace WebKit {
class WebCompositorOutputSurface;
}

namespace WebCore {
class CCInputHandler;
class IntSize;

class CCLayerTreeHostClient {
public:
    virtual void willBeginFrame() = 0;
    // Marks finishing compositing-related tasks on the main thread. In threaded mode, this corresponds to didCommit().
    virtual void didBeginFrame() = 0;
    virtual void animate(double frameBeginTime) = 0;
    virtual void layout() = 0;
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) = 0;
    virtual PassOwnPtr<WebKit::WebCompositorOutputSurface> createOutputSurface() = 0;
    virtual void didRecreateOutputSurface(bool success) = 0;
    virtual PassOwnPtr<CCInputHandler> createInputHandler() = 0;
    virtual void willCommit() = 0;
    virtual void didCommit() = 0;
    virtual void didCommitAndDrawFrame() = 0;
    virtual void didCompleteSwapBuffers() = 0;

    // Used only in the single-threaded path.
    virtual void scheduleComposite() = 0;

protected:
    virtual ~CCLayerTreeHostClient() { }
};

}

#endif // CCLayerTreeHostClient_h
