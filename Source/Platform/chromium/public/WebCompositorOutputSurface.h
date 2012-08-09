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

#ifndef WebCompositorOutputSurface_h
#define WebCompositorOutputSurface_h

namespace WebKit {

class WebCompositorFrame;
class WebGraphicsContext3D;
class WebCompositorOutputSurfaceClient;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread via WebLayerTreeViewClient::createOutputSurface.
//   2. Passed to the compositor thread and bound to a client via bindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output surface
//      (on the compositor thread) and go back to step 1.
class WebCompositorOutputSurface {
public:
    virtual ~WebCompositorOutputSurface() { }

    // Called by the compositor on the compositor thread. This is a place where thread-specific
    // data for the output surface can be initialized, since from this point on the output surface
    // will only be used on the compositor thread.
    virtual bool bindToClient(WebCompositorOutputSurfaceClient*) = 0;

    struct Capabilities {
        Capabilities()
            : hasParentCompositor(false)
        {
        }

        bool hasParentCompositor;
    };

    virtual const Capabilities& capabilities() const = 0;

    // Obtains the context associated with this output surface. In the event of a lost context, the
    // entire output surface should be recreated.
    virtual WebGraphicsContext3D* context3D() const = 0;

    // Sends frame data to the parent compositor. This should only be called
    // when capabilities().hasParentCompositor.
    virtual void sendFrameToParentCompositor(const WebCompositorFrame&) = 0;

};

}

#endif
