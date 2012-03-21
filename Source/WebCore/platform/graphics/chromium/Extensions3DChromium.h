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

#ifndef Extensions3DChromium_h
#define Extensions3DChromium_h

#include "Extensions3D.h"

namespace WebCore {

class GraphicsContext3DPrivate;
class ImageBuffer;

class Extensions3DChromium : public Extensions3D {
public:
    virtual ~Extensions3DChromium();

    // Supported extensions:
    //   GL_CHROMIUM_resource_safe  : indicating that textures/renderbuffers are always initialized before read/write.
    //   GL_CHROMIUM_strict_attribs : indicating a GL error is generated for out-of-bounds buffer accesses.
    //   GL_CHROMIUM_post_sub_buffer
    //   GL_CHROMIUM_map_sub
    //   GL_CHROMIUM_swapbuffers_complete_callback
    //   GL_CHROMIUM_rate_limit_offscreen_context
    //   GL_CHROMIUM_paint_framebuffer_canvas
    //   GL_CHROMIUM_iosurface (Mac OS X specific)
    //   GL_CHROMIUM_command_buffer_query
    //   GL_ANGLE_texture_usage
    //   GL_EXT_texture_storage
    //   GL_EXT_occlusion_query_boolean

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
    virtual String getTranslatedShaderSourceANGLE(Platform3DObject);

    enum {
        // GL_OES_EGL_image_external
        GL_TEXTURE_EXTERNAL_OES = 0x8D65,

        // GL_CHROMIUM_map_sub (enums inherited from GL_ARB_vertex_buffer_object)
        READ_ONLY = 0x88B8,
        WRITE_ONLY = 0x88B9,

        // GL_ANGLE_texture_usage
        GL_TEXTURE_USAGE_ANGLE = 0x93A2,
        GL_FRAMEBUFFER_ATTACHMENT_ANGLE = 0x93A3,

        // GL_EXT_texture_storage
        BGRA8_EXT = 0x93A1,

        // GL_EXT_occlusion_query_boolean
        ANY_SAMPLES_PASSED_EXT = 0x8C2F,
        ANY_SAMPLES_PASSED_CONSERVATIVE_EXT = 0x8D6A,
        CURRENT_QUERY_EXT = 0x8865,
        QUERY_RESULT_EXT = 0x8866,
        QUERY_RESULT_AVAILABLE_EXT = 0x8867,

        // GL_CHROMIUM_command_buffer_query
        COMMANDS_ISSUED_CHROMIUM = 0x84F2
    };

    // GL_CHROMIUM_post_sub_buffer
    void postSubBufferCHROMIUM(int x, int y, int width, int height);

    // GL_CHROMIUM_map_sub
    void* mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access);
    void unmapBufferSubDataCHROMIUM(const void*);
    void* mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access);
    void unmapTexSubImage2DCHROMIUM(const void*);

    // GL_CHROMIUM_set_visibility
    void setVisibilityCHROMIUM(bool);

    // GL_EXT_discard_framebuffer
    virtual void discardFramebufferEXT(GC3Denum target, GC3Dsizei numAttachments, const GC3Denum* attachments);
    virtual void ensureFramebufferCHROMIUM();

    // GL_CHROMIUM_gpu_memory_manager
    struct GpuMemoryAllocationCHROMIUM {
        size_t gpuResourceSizeInBytes;
        bool suggestHaveBackbuffer;

        GpuMemoryAllocationCHROMIUM(size_t gpuResourceSizeInBytes, bool suggestHaveBackbuffer)
            : gpuResourceSizeInBytes(gpuResourceSizeInBytes)
            , suggestHaveBackbuffer(suggestHaveBackbuffer)
        {
        }
    };
    class GpuMemoryAllocationChangedCallbackCHROMIUM {
    public:

        virtual void onGpuMemoryAllocationChanged(GpuMemoryAllocationCHROMIUM) = 0;
        virtual ~GpuMemoryAllocationChangedCallbackCHROMIUM() { }
    };
    void setGpuMemoryAllocationChangedCallbackCHROMIUM(PassOwnPtr<GpuMemoryAllocationChangedCallbackCHROMIUM>);

    // GL_CHROMIUM_swapbuffers_complete_callback
    class SwapBuffersCompleteCallbackCHROMIUM {
    public:
        virtual void onSwapBuffersComplete() = 0;
        virtual ~SwapBuffersCompleteCallbackCHROMIUM() { }
    };
    void setSwapBuffersCompleteCallbackCHROMIUM(PassOwnPtr<SwapBuffersCompleteCallbackCHROMIUM>);

    // GL_CHROMIUM_rate_limit_offscreen_context
    void rateLimitOffscreenContextCHROMIUM();

    // GL_CHROMIUM_paint_framebuffer_canvas
    void paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer*);

    // GL_CHROMIUM_iosurface
    // To avoid needing to expose extraneous enums, assumes internal format
    // RGBA, format BGRA, and type UNSIGNED_INT_8_8_8_8_REV.
    void texImageIOSurface2DCHROMIUM(unsigned target, int width, int height, uint32_t ioSurfaceId, unsigned plane);

    // GL_EXT_texture_storage
    void texStorage2DEXT(unsigned target, int levels, unsigned internalformat, int width, int height);

    // GL_EXT_occlusion_query
    Platform3DObject createQueryEXT();
    void deleteQueryEXT(Platform3DObject);
    GC3Dboolean isQueryEXT(Platform3DObject);
    void beginQueryEXT(GC3Denum, Platform3DObject);
    void endQueryEXT(GC3Denum);
    void getQueryivEXT(GC3Denum, GC3Denum, GC3Dint*);
    void getQueryObjectuivEXT(Platform3DObject, GC3Denum, GC3Duint*);

private:
    // Instances of this class are strictly owned by the GraphicsContext3D implementation and do not
    // need to be instantiated by any other code.
    friend class GraphicsContext3DPrivate;
    explicit Extensions3DChromium(GraphicsContext3DPrivate*);

    // Weak pointer back to GraphicsContext3DPrivate
    GraphicsContext3DPrivate* m_private;
};

} // namespace WebCore

#endif // Extensions3DChromium_h
