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

#ifndef Extensions3DChromium_h
#define Extensions3DChromium_h

#include "Extensions3D.h"

namespace WebCore {

class GraphicsContext3DInternal;

class Extensions3DChromium : public Extensions3D {
public:
    virtual ~Extensions3DChromium();

    // Supported extensions:
    //   GL_CHROMIUM_resource_safe  : indicating that textures/renderbuffers are always initialized before read/write.
    //   GL_CHROMIUM_strict_attribs : indicating a GL error is generated for out-of-bounds buffer accesses.
    //   GL_CHROMIUM_map_sub
    //   GL_CHROMIUM_copy_texture_to_parent_texture
    //   GL_CHROMIUM_latch
    //   GL_CHROMIUM_swapbuffers_complete_callback
    //   GL_CHROMIUM_rate_limit_offscreen_context

    // Extensions3D methods.
    virtual bool supports(const String&);
    virtual void ensureEnabled(const String&);
    virtual bool isEnabled(const String&);
    virtual int getGraphicsResetStatusARB();
    virtual void blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter);
    virtual void renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height);
    virtual Platform3DObject createVertexArrayOES();
    virtual void deleteVertexArrayOES(Platform3DObject);
    virtual GC3Dboolean isVertexArrayOES(Platform3DObject);
    virtual void bindVertexArrayOES(Platform3DObject);

    enum {
        // GL_CHROMIUM_map_sub (enums inherited from GL_ARB_vertex_buffer_object)
        READ_ONLY = 0x88B8,
        WRITE_ONLY = 0x88B9
    };

    // GL_CHROMIUM_map_sub
    void* mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access);
    void unmapBufferSubDataCHROMIUM(const void*);
    void* mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access);
    void unmapTexSubImage2DCHROMIUM(const void*);

    // GL_CHROMIUM_copy_texture_to_parent_texture
    void copyTextureToParentTextureCHROMIUM(unsigned texture, unsigned parentTexture);

    // GL_CHROMIUM_latch
    void getParentToChildLatchCHROMIUM(GC3Duint* latchId);
    void getChildToParentLatchCHROMIUM(GC3Duint* latchId);
    void waitLatchCHROMIUM(GC3Duint latchId);
    void setLatchCHROMIUM(GC3Duint latchId);

    // GL_CHROMIUM_swapbuffers_complete_callback
    class SwapBuffersCompleteCallbackCHROMIUM {
    public:
        virtual void onSwapBuffersComplete() = 0;
        virtual ~SwapBuffersCompleteCallbackCHROMIUM() { }
    };
    void setSwapBuffersCompleteCallbackCHROMIUM(PassOwnPtr<SwapBuffersCompleteCallbackCHROMIUM>);

    // GL_CHROMIUM_rate_limit_offscreen_context
    void rateLimitOffscreenContextCHROMIUM();

private:
    // Instances of this class are strictly owned by the GraphicsContext3D implementation and do not
    // need to be instantiated by any other code.
    friend class GraphicsContext3DInternal;
    explicit Extensions3DChromium(GraphicsContext3DInternal*);

    // Weak pointer back to GraphicsContext3DInternal
    GraphicsContext3DInternal* m_internal;
};

} // namespace WebCore

#endif // Extensions3DChromium_h
