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

#ifndef WebLayerTreeViewClient_h
#define WebLayerTreeViewClient_h

namespace WebKit {
class WebCompositorOutputSurface;
struct WebSize;
class WebThread;

class WebLayerTreeViewClient {
public:
    // Indicates to the embedder that the compositor is about to begin a
    // frame. This is is a signal to flow control mechanisms that a frame is
    // beginning. This call will be followed by updateAnimations and then
    // layout, which should be used for actual animation or tree manipulation
    // tasks.  FIXME: make pure virtual once upstream deps are satisfied.
    virtual void willBeginFrame() { }

    // Indicates that main thread tasks associated with frame rendering have completed.
    // Issued unconditionally, even if the context was lost in the process.
    virtual void didBeginFrame() { }

    // Updates animation and layout. These are called before the compositing
    // pass so that layers can be updated at the given frame time.
    virtual void updateAnimations(double monotonicFrameBeginTime) = 0;
    virtual void layout() = 0;

    // Applies a scroll delta to the root layer, which is bundled with a page
    // scale factor that may apply a CSS transform on the whole document (used
    // for mobile-device pinch zooming). This is triggered by events sent to the
    // compositor thread through the WebCompositor interface.
    virtual void applyScrollAndScale(const WebSize& scrollDelta, float scaleFactor) = 0;

    // DEPRECATED: Creates a 3D context suitable for the compositing. This may be called
    // more than once if the context gets lost. This will be removed once
    // downstream dependencies have been removed.
    virtual WebGraphicsContext3D* createContext3D() { return 0; }

    // DEPRECATED: Signals a successful rebinding of the 3D context (e.g. after a lost
    // context event).
    virtual void didRebindGraphicsContext(bool) { return; }

    // Creates the output surface. This may be called more than once
    // if the context gets lost.
    virtual WebCompositorOutputSurface* createOutputSurface() { return 0; }

    // Signals a successful recreation of the output surface (e.g. after a lost
    // 3D context event).
    virtual void didRecreateOutputSurface(bool success) { }

    // Indicates that a frame will be committed to the impl side of the compositor
    // for rendering.
    virtual void willCommit() { }

    // Indicates that a frame was committed to the impl side of the compositor
    // for rendering.
    //
    // FIXME: make this non-virtual when ui/compositor DEP is resolved.
    virtual void didCommit() { }

    // Indicates that a frame was committed to the impl side and drawing
    // commands for it were issued to the GPU.
    virtual void didCommitAndDrawFrame() = 0;

    // Indicates that a frame previously issued to the GPU has completed
    // rendering.
    virtual void didCompleteSwapBuffers() = 0;

    // Schedules a compositing pass, meaning the client should call
    // WebLayerTreeView::composite at a later time. This is only called if the
    // compositor thread is disabled; when enabled, the compositor will
    // internally schedule a compositing pass when needed.
    virtual void scheduleComposite() = 0;

protected:
    virtual ~WebLayerTreeViewClient() { }
};

} // namespace WebKit

#endif // WebLayerTreeViewClient_h
