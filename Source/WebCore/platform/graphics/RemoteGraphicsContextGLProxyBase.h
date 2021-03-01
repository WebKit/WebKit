/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

#include "ExtensionsGL.h"
#include "GraphicsContextGL.h"

#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS WebGLLayer;
#endif

namespace WebCore {

#if PLATFORM(COCOA)
class GraphicsContextGLIOSurfaceSwapChain;
#endif

// A base class for RemoteGraphicsContextGL proxy side implementation
// This implements the parts that are using WebCore internal functionality:
// - Drawing buffer tracking management.
// - Compositing support.
class WEBCORE_EXPORT RemoteGraphicsContextGLProxyBase : public GraphicsContextGL, public ExtensionsGL {
public:
    RemoteGraphicsContextGLProxyBase(const GraphicsContextGLAttributes&);
    ~RemoteGraphicsContextGLProxyBase() override;

    // Other WebCore::GraphicsContextGL overrides.
    using GraphicsContextGL::isEnabled;
    PlatformLayer* platformLayer() const final;
    ExtensionsGL& getExtensions() final;
    void setContextVisibility(bool) final;
    bool isGLES2Compliant() const final;
    void markContextChanged() final;
    bool layerComposited() const final;
    void setBuffersToAutoClear(GCGLbitfield) final;
    GCGLbitfield getBuffersToAutoClear() const final;
    void markLayerComposited() final;
    void enablePreserveDrawingBuffer() final;

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    GraphicsContextGLCV* asCV() final;
#endif
    // Other ExtensionGL overrides.
    using ExtensionsGL::isEnabled;
    bool supports(const String&) final;
    void ensureEnabled(const String&) final;
    bool isEnabled(const String&) final;

#if !USE(ANGLE)
    void readnPixelsEXT(GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, GCGLvoid*) final;
    void getnUniformfvEXT(GCGLuint, GCGLint, GCGLsizei, GCGLfloat*) final;
    void getnUniformivEXT(GCGLuint, GCGLint, GCGLsizei, GCGLint*) final;
#endif

protected:
    void initialize(const String& availableExtensions, const String& requestableExtensions);
    virtual void waitUntilInitialized() = 0;
    virtual void ensureExtensionEnabled(const String&) = 0;
    virtual void notifyMarkContextChanged() = 0;
#if PLATFORM(COCOA)
    GraphicsContextGLIOSurfaceSwapChain& platformSwapChain();
#endif

private:
    void platformInitialize();
#if PLATFORM(COCOA)
    RetainPtr<WebGLLayer> m_webGLLayer;
#endif

    // Guarded by waitUntilInitialized().
    HashSet<String> m_availableExtensions;
    HashSet<String> m_requestableExtensions;

    HashSet<String> m_enabledExtensions;

    // FIXME: Drawing buffer clear tracking should be moved to the client.
    bool m_layerComposited = false;
    GCGLbitfield m_buffersToAutoClear = { 0 };
};

}
#endif
