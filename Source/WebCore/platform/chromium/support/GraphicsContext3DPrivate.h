/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "SkBitmap.h"
#include <wtf/HashSet.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>

class GrContext;

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class DrawingBuffer;
class Extensions3DChromium;
class GraphicsContext3DContextLostCallbackAdapter;
class GraphicsContext3DErrorMessageCallbackAdapter;
class GrMemoryAllocationChangedCallbackAdapter;

class GraphicsContext3DPrivate {
public:
    // Callers must make the context current before using it AND check that the context was created successfully
    // via ContextLost before using the context in any way. Once made current on a thread, the context cannot
    // be used on any other thread.
    static PassRefPtr<GraphicsContext3D> createGraphicsContextFromWebContext(PassOwnPtr<WebKit::WebGraphicsContext3D>, bool preserveDrawingBuffer = false);

    static PassRefPtr<GraphicsContext3D> createGraphicsContextFromExternalWebContextAndGrContext(WebKit::WebGraphicsContext3D*, GrContext*, bool preserveDrawingBuffer = false);

    // Helper function to provide access to the lower-level WebGraphicsContext3D,
    // which is needed for subordinate contexts like WebGL's to share resources
    // with the compositor's context.
    static WebKit::WebGraphicsContext3D* extractWebGraphicsContext3D(GraphicsContext3D*);

    virtual ~GraphicsContext3DPrivate();

    WebKit::WebGraphicsContext3D* webContext() const { return m_impl; }

    GrContext* grContext();

    void markContextChanged();
    bool layerComposited() const;
    void markLayerComposited();

    void paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer*);

    void setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback>);
    void setErrorMessageCallback(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback>);

    // Extensions3D support.
    Extensions3D* getExtensions();
    bool supportsExtension(const String& name);
    bool ensureExtensionEnabled(const String& name);
    bool isExtensionEnabled(const String& name);

    bool isResourceSafe();

    bool preserveDrawingBuffer() const { return m_preserveDrawingBuffer; }

private:
    GraphicsContext3DPrivate(PassOwnPtr<WebKit::WebGraphicsContext3D>, bool preserveDrawingBuffer);
    GraphicsContext3DPrivate(WebKit::WebGraphicsContext3D*, GrContext*, bool preserveDrawingBuffer);

    void initializeExtensions();

    WebKit::WebGraphicsContext3D* m_impl;
    OwnPtr<WebKit::WebGraphicsContext3D> m_ownedWebContext;
    OwnPtr<Extensions3DChromium> m_extensions;
    OwnPtr<GraphicsContext3DContextLostCallbackAdapter> m_contextLostCallbackAdapter;
    OwnPtr<GraphicsContext3DErrorMessageCallbackAdapter> m_errorMessageCallbackAdapter;
    OwnPtr<GrMemoryAllocationChangedCallbackAdapter> m_grContextMemoryAllocationCallbackAdapter;
    bool m_initializedAvailableExtensions;
    HashSet<String> m_enabledExtensions;
    HashSet<String> m_requestableExtensions;
    bool m_layerComposited;
    bool m_preserveDrawingBuffer;

    enum ResourceSafety {
        ResourceSafetyUnknown,
        ResourceSafe,
        ResourceUnsafe
    };
    ResourceSafety m_resourceSafety;

    // If the width and height of the Canvas's backing store don't
    // match those that we were given in the most recent call to
    // reshape(), then we need an intermediate bitmap to read back the
    // frame buffer into. This seems to happen when CSS styles are
    // used to resize the Canvas.
    SkBitmap m_resizingBitmap;

    GrContext* m_grContext;
    SkAutoTUnref<GrContext> m_ownedGrContext;
};

} // namespace WebCore

#endif // GraphicsContext3DPrivate_h
