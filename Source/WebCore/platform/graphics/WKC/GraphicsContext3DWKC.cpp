/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2011, 2012 ACCESS CO., LTD. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

#include "BitmapImage.h"
#include "CanvasRenderingContext.h"
#include "CString.h"
#include "Extensions3D.h"
#include "FastMalloc.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "ImageWKC.h"
#include "WebGLRenderingContext.h"

#include <wkc/wkcpeer.h>
#include <wkc/wkcglpeer.h>
#include <wkc/wkcgpeer.h>

#include "NotImplemented.h"

namespace WebCore {

class Extensions3DWKC : public Extensions3D
{
public:
    Extensions3DWKC(void* peer)
        : m_peer(peer) {}
    ~Extensions3DWKC() {}

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

private:
    void* m_peer;
};

class GraphicsContext3DPrivate
{
public:
    static PassOwnPtr<GraphicsContext3DPrivate> create(GraphicsContext3D::Attributes& attrs, HostWindow* hostwindow, bool in_direct);
    ~GraphicsContext3DPrivate()
    {
        if (m_data)
            fastFree(m_data);
        if (m_layer)
            wkcLayerDeletePeer(m_layer);
    }

    PlatformGraphicsContext3D platformGraphicsContext3D();
    inline void* peer() { return m_peer; }

    GraphicsContext3D::ContextLostCallback* contextLostCallback() const { return m_contextLostCallback.get(); }
    void setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> cb) { m_contextLostCallback = cb; }

    inline GraphicsContext3D::Attributes& attrs() { return m_attrs; }
    inline Extensions3D* extensions() const { return m_extensions; }
    inline void* layer() const { return m_layer; }

    void reshape(int width, int height);

private:
    GraphicsContext3DPrivate()
        : m_peer(0)
        , m_texture(0)
        , m_contextLostCallback()
        , m_attrs()
        , m_extensions(0)
        , m_data(0)
        , m_width(0)
        , m_height(0)
    {
        m_layer = wkcLayerNewPeer(WKC_LAYER_TYPE_3DCANVAS, 0, 1.f);
    }

private:
    void* m_peer;
    Platform3DObject m_texture;
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
    GraphicsContext3D::Attributes m_attrs;
    Extensions3DWKC* m_extensions;
    void* m_data;
    int m_width;
    int m_height;
    void* m_layer;
};

PassOwnPtr<GraphicsContext3DPrivate>
GraphicsContext3DPrivate::create(GraphicsContext3D::Attributes& attrs, HostWindow* hostwindow, bool in_direct)
{
    WKCGLAttributes ga = {
        attrs.alpha,
        attrs.depth,
        attrs.stencil,
        attrs.antialias,
        attrs.premultipliedAlpha,
        attrs.preserveDrawingBuffer,
        attrs.noExtensions,
        attrs.shareResources,
        attrs.preferDiscreteGPU,
    };

    OwnPtr<GraphicsContext3DPrivate> self = adoptPtr(new GraphicsContext3DPrivate());

    self->m_peer = wkcGLCreateContextPeer(&ga, (void *)hostwindow);
    if (!self->m_peer) {
        return nullptr;
    }
    self->m_extensions = new Extensions3DWKC(self->m_peer);
    self->m_attrs = attrs;

    attrs.depth = ga.fDepth;
    attrs.stencil = ga.fStencil;
    attrs.antialias = ga.fAntialias;

    return self.release();
}

void
GraphicsContext3DPrivate::reshape(int width, int height)
{
    if (width==m_width && height==m_height)
        return;


    if (m_data) {
        WTF::fastFree(m_data);
        m_data = 0;
        m_width = m_height = 0;
    }

    m_width = width;
    m_height = height;
    int len = 4*width*height;
    WTF::TryMallocReturnValue ret = WTF::tryFastMalloc(len);
    if (!ret.getValue(m_data)) {
        wkcMemoryNotifyMemoryAllocationErrorPeer(len, WKC_MEMORYALLOC_TYPE_IMAGE);
    }
}

#define peer() (m_private->peer())

GraphicsContext3D::GraphicsContext3D(Attributes attrs, HostWindow* hostwindow, bool in_direct)
    : m_currentWidth(0)
    , m_currentHeight(0)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
    if (peer()) {
        wkcGLDeleteContextPeer(peer());
    }
}

PassRefPtr<GraphicsContext3D>
GraphicsContext3D::create(Attributes attrs, HostWindow* hostwindow, RenderStyle style)
{
    if (style == RenderDirectlyToHostWindow)
        return 0;

    OwnPtr<GraphicsContext3DPrivate> ctx = GraphicsContext3DPrivate::create(attrs, hostwindow, false);
    if (!ctx)
        return 0;

    RefPtr<GraphicsContext3D> self = adoptRef(new GraphicsContext3D(attrs, hostwindow, false));
    self->m_private = ctx.release();
    return self.release();
}

void
GraphicsContext3D::contextLostProc(void *in_opaque)
{
    GraphicsContext3D* self = static_cast<GraphicsContext3D *>(in_opaque);
    self->m_private->contextLostCallback()->onContextLost();
}

void
GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback> cb)
{
    m_private->setContextLostCallback(cb);
    wkcGLSetContextLostProcPeer(peer(), contextLostProc, this);
}

void
GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback> cb)
{
    notImplemented();
}

bool
GraphicsContext3D::makeContextCurrent()
{
    wkcGLMakeContextCurrentPeer(peer());
    return true;
}

bool
GraphicsContext3D::isGLES2Compliant() const
{
    return wkcGLIsGLES20CompliantPeer(peer());
}

// WebGL entry points

void
GraphicsContext3D::activeTexture(GC3Denum texture)
{
    makeContextCurrent();
    wkcGLActiveTexturePeer(peer(), texture);
}


void
GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    wkcGLAttachShaderPeer(peer(), program, shader);
}


void
GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    ASSERT(program);
    makeContextCurrent();
    wkcGLBindAttribLocationPeer(peer(), program, index, name.utf8().data());
}


void
GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLBindBufferPeer(peer(), target, obj);
}


void
GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLBindFramebufferPeer(peer(), target, obj);
}


void
GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLBindRenderbufferPeer(peer(), target, obj);
}


void
GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject obj)
{
    makeContextCurrent();    
    wkcGLBindTexturePeer(peer(), target, obj);
}


void
GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    wkcGLBlendColorPeer(peer(), red, green, blue, alpha);
}


void
GraphicsContext3D::blendEquation(GC3Denum mode)
{
    makeContextCurrent();
    wkcGLBlendEquationPeer(peer(), mode);
}


void
GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    makeContextCurrent();
    wkcGLBlendEquationSeparatePeer(peer(), modeRGB, modeAlpha);
}


void
GraphicsContext3D::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    makeContextCurrent();
    wkcGLBlendFuncPeer(peer(), sfactor, dfactor);
}


void
GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    makeContextCurrent();
    wkcGLBlendFuncSeparatePeer(peer(), srcRGB, dstRGB, srcAlpha, dstAlpha);
}


void
GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    makeContextCurrent();
    wkcGLBufferDataPeer(peer(), target, size, 0, usage);
}


void
GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    makeContextCurrent();
    wkcGLBufferDataPeer(peer(), target, size, data, usage);
}


void
GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    makeContextCurrent();
    wkcGLBufferSubDataPeer(peer(), target, offset, size, data);
}



GC3Denum
GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    makeContextCurrent();
    return wkcGLCheckFramebufferStatusPeer(peer(), target);
}


void
GraphicsContext3D::clear(GC3Dbitfield mask)
{
    makeContextCurrent();
    wkcGLClearPeer(peer(), mask);
}


void
GraphicsContext3D::clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    wkcGLClearColorPeer(peer(), red, green, blue, alpha);
}


void
GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
    wkcGLClearDepthPeer(peer(), depth);
}


void
GraphicsContext3D::clearStencil(GC3Dint s)
{
    makeContextCurrent();
    wkcGLClearStencilPeer(peer(), s);
}


void
GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    makeContextCurrent();
    wkcGLColorMaskPeer(peer(), red, green, blue, alpha);
}


void
GraphicsContext3D::compileShader(Platform3DObject shader)
{
    ASSERT(shader);
    makeContextCurrent();

    wkcGLCompileShaderPeer(peer(), shader);
}

void
GraphicsContext3D::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data)
{
    notImplemented();
}

void
GraphicsContext3D::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data)
{
    notImplemented();
}

void
GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    wkcGLCopyTexImage2DPeer(peer(), target, level, internalformat, x, y, width, height, border);
}


void
GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    wkcGLCopyTexSubImage2DPeer(peer(), target, level, xoffset, yoffset, x, y, width, height);
}


void
GraphicsContext3D::cullFace(GC3Denum mode)
{
    makeContextCurrent();
    wkcGLCullFacePeer(peer(), mode);
}


void
GraphicsContext3D::depthFunc(GC3Denum func)
{
    makeContextCurrent();
    wkcGLDepthFuncPeer(peer(), func);
}


void
GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    makeContextCurrent();
    wkcGLDepthMaskPeer(peer(), flag);
}


void
GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
    wkcGLDepthRangePeer(peer(), zNear, zFar);
}


void
GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    wkcGLDetachShaderPeer(peer(), program, shader);
}


void
GraphicsContext3D::disable(GC3Denum cap)
{
    makeContextCurrent();
    wkcGLDisablePeer(peer(), cap);
}


void
GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    wkcGLDisableVertexAttribArrayPeer(peer(), index);
}


void
GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    makeContextCurrent();
    wkcGLDrawArraysPeer(peer(), mode, first, count);
}


void
GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    makeContextCurrent();
    wkcGLDrawElementsPeer(peer(), mode, count, type, offset);
}


void
GraphicsContext3D::enable(GC3Denum cap)
{
    makeContextCurrent();
    wkcGLEnablePeer(peer(), cap);
}


void
GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    wkcGLEnableVertexAttribArrayPeer(peer(), index);
}


void
GraphicsContext3D::finish()
{
    makeContextCurrent();
    wkcGLFinishPeer(peer());
}


void
GraphicsContext3D::flush()
{
    makeContextCurrent();
    wkcGLFlushPeer(peer());
}


void
GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLFramebufferRenderbufferPeer(peer(), target, attachment, renderbuffertarget, obj);
}


void
GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject obj, GC3Dint level)
{
    makeContextCurrent();
    wkcGLFramebufferTexture2DPeer(peer(), target, attachment, textarget, obj, level);
}


void
GraphicsContext3D::frontFace(GC3Denum mode)
{
    makeContextCurrent();
    wkcGLFrontFacePeer(peer(), mode);
}


void
GraphicsContext3D::generateMipmap(GC3Denum target)
{
    makeContextCurrent();
    wkcGLGenerateMipmapPeer(peer(), target);
}


bool
GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();

    GC3Dint len = 0;
    wkcGLGetProgramivPeer(peer(), program, 0x8b8a /*GL_ACTIVE_ATTRIBUTE_MAX_LENGTH*/, &len);
    if (!len)
        return false;

    WTF::TryMallocReturnValue rv = WTF::tryFastMalloc(len);
    char* name = 0;
    GC3Dint nameLength = 0;
    if (!rv.getValue(name))
        return false;

    wkcGLGetActiveAttribPeer(peer(), program, index, len, &nameLength, &info.size, &info.type, name);
    if (!nameLength) {
        fastFree(name);
        return false;
    }
    info.name = String::fromUTF8(name, nameLength);
    fastFree(name);
    return true;
}


bool
GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();

    GC3Dint len = 0;
    wkcGLGetProgramivPeer(peer(), program, 0x8b87 /*GL_ACTIVE_UNIFORM_MAX_LENGTH*/, &len);
    if (!len)
        return false;

    WTF::TryMallocReturnValue rv = WTF::tryFastMalloc(len);
    char* name = 0;
    GC3Dint nameLength = 0;
    if (!rv.getValue(name))
        return false;

    wkcGLGetActiveUniformPeer(peer(), program, index, len, &nameLength, &info.size, &info.type, name);
    if (!nameLength) {
        fastFree(name);
        return false;
    }
    info.name = String::fromUTF8(name, nameLength);
    fastFree(name);
    return true;
}


void
GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    makeContextCurrent();
    wkcGLGetAttachedShadersPeer(peer(), program, maxCount, count, shaders);
}


GC3Dint
GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;
    makeContextCurrent();
    return wkcGLGetAttribLocationPeer(peer(), program, name.utf8().data());
}


void
GraphicsContext3D::getBooleanv(GC3Denum pname, GC3Dboolean* value)
{
    makeContextCurrent();
    wkcGLGetBooleanvPeer(peer(), pname, value);
}


void
GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    wkcGLGetBufferParameterivPeer(peer(), target, pname, value);
}


GraphicsContext3D::Attributes
GraphicsContext3D::getContextAttributes()
{
    return m_private->attrs();
}


GC3Denum
GraphicsContext3D::getError()
{
    makeContextCurrent();
    return wkcGLGetErrorPeer(peer());
}


void
GraphicsContext3D::getFloatv(GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLGetFloatvPeer(peer(), pname, value);
}


void
GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    wkcGLGetFramebufferAttachmentParameterivPeer(peer(), target, attachment, pname, value);
}


void
GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    wkcGLGetIntegervPeer(peer(), pname, value);
}


void
GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    wkcGLGetProgramivPeer(peer(), program, pname, value);
}


String
GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return "";
    }
    makeContextCurrent();

    GC3Dint len = 0;
    wkcGLGetProgramivPeer(peer(), program, 0x8b84 /*GL_INFO_LOG_LENGTH*/, &len);
    if (!len)
        return "";

    WTF::TryMallocReturnValue rv = WTF::tryFastMalloc(len);
    char* info = 0;
    if (!rv.getValue(info))
        return "";

    wkcGLGetProgramInfoLogPeer(peer(), program, len, &len, info);
    String ret(info);
    fastFree(info);
    return ret;
}


void
GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    wkcGLGetRenderbufferParameterivPeer(peer(), target, pname, value);
}


void
GraphicsContext3D::getShaderiv(Platform3DObject obj, GC3Denum pname, GC3Dint* value)
{
    wkcGLGetShaderivPeer(peer(), obj, pname, value);
}


String
GraphicsContext3D::getShaderInfoLog(Platform3DObject program)
{
    GC3Dint len = 0;
    wkcGLGetShaderivPeer(peer(), program, 0x8b84 /*GL_INFO_LOG_LENGTH*/, &len);

    char* name = 0;
    CString cs = CString::newUninitialized(len+1, name);

    wkcGLGetShaderInfoLogPeer(peer(), program, len+1, &len, name);
    return cs.data();
}


String
GraphicsContext3D::getShaderSource(Platform3DObject program)
{
    GC3Dint len = 0;
    wkcGLGetShaderivPeer(peer(), program, 0x8b88 /*GL_SHADER_SOURCE_LENGTH*/, &len);

    char* name = 0;
    CString cs = CString::newUninitialized(len+1, name);

    wkcGLGetShaderSourcePeer(peer(), program, len+1, &len, name);
    return cs.data();
}


String
GraphicsContext3D::getString(GC3Denum name)
{
    makeContextCurrent();
    return String(reinterpret_cast<const char *>(wkcGLGetStringPeer(peer(), name)));
}


void
GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLGetTexParameterfvPeer(peer(), target, pname, value);
}


void
GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    wkcGLGetTexParameterivPeer(peer(), target, pname, value);
}


void
GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLGetUniformfvPeer(peer(), program, location, value);
}


void
GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    wkcGLGetUniformivPeer(peer(), program, location, value);
}


GC3Dint
GraphicsContext3D::getUniformLocation(Platform3DObject obj, const String& name)
{
    makeContextCurrent();
    return wkcGLGetUniformLocationPeer(peer(), obj, name.ascii().data());
}


void
GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLGetVertexAttribfvPeer(peer(), index, pname, value);
}


void
GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    wkcGLGetVertexAttribivPeer(peer(), index, pname, value);
}

GC3Dsizeiptr
GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum pname)
{
    makeContextCurrent();
    return wkcGLGetVertexAttribOffsetPeer(peer(), index, pname);
}


void
GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    makeContextCurrent();
    wkcGLHintPeer(peer(), target, mode);
}


GC3Dboolean
GraphicsContext3D::isBuffer(Platform3DObject obj)
{
    makeContextCurrent();
    return wkcGLIsBufferPeer(peer(), obj);
}


GC3Dboolean
GraphicsContext3D::isEnabled(GC3Denum cap)
{
    makeContextCurrent();
    return wkcGLIsEnabledPeer(peer(), cap);
}


GC3Dboolean
GraphicsContext3D::isFramebuffer(Platform3DObject obj)
{
    makeContextCurrent();
    return wkcGLIsFramebufferPeer(peer(), obj);
}


GC3Dboolean
GraphicsContext3D::isProgram(Platform3DObject obj)
{
    makeContextCurrent();
    return wkcGLIsProgramPeer(peer(), obj);
}


GC3Dboolean
GraphicsContext3D::isRenderbuffer(Platform3DObject obj)
{
    makeContextCurrent();
    return wkcGLIsRenderbufferPeer(peer(), obj);
}


GC3Dboolean
GraphicsContext3D::isShader(Platform3DObject obj)
{
    makeContextCurrent();
    return wkcGLIsShaderPeer(peer(), obj);
}


GC3Dboolean
GraphicsContext3D::isTexture(Platform3DObject obj)
{
    makeContextCurrent();
    return wkcGLIsTexturePeer(peer(), obj);
}


void
GraphicsContext3D::lineWidth(GC3Dfloat val)
{
    makeContextCurrent();
    wkcGLLineWidthPeer(peer(), val);
}


void
GraphicsContext3D::linkProgram(Platform3DObject program)
{
    ASSERT(program);
    makeContextCurrent();
    wkcGLLinkProgramPeer(peer(), program);
}


void
GraphicsContext3D::pixelStorei(GC3Denum pname, GC3Dint param)
{
    makeContextCurrent();
    wkcGLPixelStoreiPeer(peer(), pname, param);
}


void
GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    makeContextCurrent();
    wkcGLPolygonOffsetPeer(peer(), factor, units);
}


void
GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    makeContextCurrent();
    wkcGLReadPixelsPeer(peer(), x, y, width, height, format, type, data);
}


void
GraphicsContext3D::releaseShaderCompiler()
{
    makeContextCurrent();
    wkcGLReleaseShaderCompilerPeer(peer());
}


void
GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    wkcGLRenderbufferStoragePeer(peer(), target, internalformat, width, height);
}


void
GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    makeContextCurrent();
    wkcGLSampleCoveragePeer(peer(), value, invert);
}


void
GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    wkcGLScissorPeer(peer(), x, y, width, height);
}


void
GraphicsContext3D::shaderSource(Platform3DObject obj, const String& string)
{
    makeContextCurrent();
    wkcGLShaderSourcePeer(peer(), obj, string.utf8().data());
}


void
GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    wkcGLStencilFuncPeer(peer(), func, ref, mask);
}


void
GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    wkcGLStencilFuncSeparatePeer(peer(), face, func, ref, mask);
}


void
GraphicsContext3D::stencilMask(GC3Duint mask)
{
    makeContextCurrent();
    wkcGLStencilMaskPeer(peer(), mask);
}


void
GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    makeContextCurrent();
    wkcGLStencilMaskSeparatePeer(peer(), face, mask);
}


void
GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    wkcGLStencilOpPeer(peer(), fail, zfail, zpass);
}


void
GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    wkcGLStencilOpSeparatePeer(peer(), face, fail, zfail, zpass);
}


bool
GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    wkcGLTexImage2DPeer(peer(), target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}


void
GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param)
{
    makeContextCurrent();
    wkcGLTexParameterfPeer(peer(), target, pname, param);
}


void
GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param)
{
    makeContextCurrent();
    wkcGLTexParameteriPeer(peer(), target, pname, param);
}


void
GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    wkcGLTexSubImage2DPeer(peer(), target, level, xoffset, yoffset, width, height, format, type, pixels);
}



void
GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat x)
{
    makeContextCurrent();
    wkcGLUniform1fPeer(peer(), location, x);
}


void
GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    makeContextCurrent();
    wkcGLUniform1fvPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint x)
{
    makeContextCurrent();
    wkcGLUniform1iPeer(peer(), location, x);
}


void
GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    makeContextCurrent();
    wkcGLUniform1ivPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat x, float y)
{
    makeContextCurrent();
    wkcGLUniform2fPeer(peer(), location, x, y);
}


void
GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    makeContextCurrent();
    wkcGLUniform2fvPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint x, GC3Dint y)
{
    makeContextCurrent();
    wkcGLUniform2iPeer(peer(), location, x, y);
}


void
GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    makeContextCurrent();
    wkcGLUniform2ivPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    makeContextCurrent();
    wkcGLUniform3fPeer(peer(), location, x, y, z);
}


void
GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    makeContextCurrent();
    wkcGLUniform3fvPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    makeContextCurrent();
    wkcGLUniform3iPeer(peer(), location, x, y, z);
}


void
GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    makeContextCurrent();
    wkcGLUniform3ivPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    makeContextCurrent();
    wkcGLUniform4fPeer(peer(), location, x, y, z, w);
}


void
GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* v)
{
    makeContextCurrent();
    wkcGLUniform4fvPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    makeContextCurrent();
    wkcGLUniform4iPeer(peer(), location, x, y, z, w);
}


void
GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dsizei size, GC3Dint* v)
{
    makeContextCurrent();
    wkcGLUniform4ivPeer(peer(), location, size, v);
}


void
GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLUniformMatrix2fvPeer(peer(), location, size, transpose, value);
}


void
GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLUniformMatrix3fvPeer(peer(), location, size, transpose, value);
}


void
GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    makeContextCurrent();
    wkcGLUniformMatrix4fvPeer(peer(), location, size, transpose, value);
}


void
GraphicsContext3D::useProgram(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLUseProgramPeer(peer(), obj);
}


void
GraphicsContext3D::validateProgram(Platform3DObject obj)
{
    ASSERT(obj);
    makeContextCurrent();
    wkcGLValidateProgramPeer(peer(), obj);
}


void
GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat x)
{
    makeContextCurrent();
    wkcGLVertexAttrib1fPeer(peer(), index, x);
}


void
GraphicsContext3D::vertexAttrib1fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    wkcGLVertexAttrib1fvPeer(peer(), index, values);
}


void
GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y)
{
    makeContextCurrent();
    wkcGLVertexAttrib2fPeer(peer(), index, x, y);
}


void
GraphicsContext3D::vertexAttrib2fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    wkcGLVertexAttrib2fvPeer(peer(), index, values);
}


void
GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    makeContextCurrent();
    wkcGLVertexAttrib3fPeer(peer(), index, x, y, z);
}


void
GraphicsContext3D::vertexAttrib3fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    wkcGLVertexAttrib3fvPeer(peer(), index, values);
}


void
GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    makeContextCurrent();
    wkcGLVertexAttrib4fPeer(peer(), index, x, y, z, w);
}


void
GraphicsContext3D::vertexAttrib4fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    wkcGLVertexAttrib4fvPeer(peer(), index, values);
}


void
GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized,
                                       GC3Dsizei stride, GC3Dintptr offset)
{
    makeContextCurrent();
    wkcGLVertexAttribPointerPeer(peer(), index, size, type, normalized, stride, offset);
}


void
GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    wkcGLViewportPeer(peer(), x, y, width, height);
}


void
GraphicsContext3D::reshape(int width, int height)
{
    m_private->reshape(width, height);

    makeContextCurrent();
    wkcGLReshapePeer(peer(), width, height);
}

bool
GraphicsContext3D::isResourceSafe()
{
    return true;
}

void
GraphicsContext3D::markContextChanged()
{
    wkcGLMarkContextChangedPeer(peer());
}


void
GraphicsContext3D::markLayerComposited()
{
    wkcGLMarkLayerCompositedPeer(peer());
}


bool
GraphicsContext3D::layerComposited() const
{
    return wkcGLLayerCompositedPeer(peer());
}

Platform3DObject
GraphicsContext3D::createBuffer()
{
    makeContextCurrent();
    return wkcGLCreateBufferPeer(peer());
}


Platform3DObject
GraphicsContext3D::createFramebuffer()
{
    makeContextCurrent();
    return wkcGLCreateFramebufferPeer(peer());
}


Platform3DObject
GraphicsContext3D::createProgram()
{
    makeContextCurrent();
    return wkcGLCreateProgramPeer(peer());
}


Platform3DObject
GraphicsContext3D::createRenderbuffer()
{
    makeContextCurrent();
    return wkcGLCreateRenderbufferPeer(peer());
}


Platform3DObject
GraphicsContext3D::createShader(GC3Denum type)
{
    makeContextCurrent();
    return wkcGLCreateShaderPeer(peer(), type);
}


Platform3DObject
GraphicsContext3D::createTexture()
{
    makeContextCurrent();
    return wkcGLCreateTexturePeer(peer());
}


void
GraphicsContext3D::deleteBuffer(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLDeleteBufferPeer(peer(), obj);
}


void
GraphicsContext3D::deleteFramebuffer(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLDeleteFramebufferPeer(peer(), obj);
}


void
GraphicsContext3D::deleteProgram(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLDeleteProgramPeer(peer(), obj);
}


void
GraphicsContext3D::deleteRenderbuffer(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLDeleteRenderbufferPeer(peer(), obj);
}


void
GraphicsContext3D::deleteShader(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLDeleteShaderPeer(peer(), obj);
}


void
GraphicsContext3D::deleteTexture(Platform3DObject obj)
{
    makeContextCurrent();
    wkcGLDeleteTexturePeer(peer(), obj);
}



void
GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    wkcGLSynthesizeGLErrorPeer(peer(), error);
}

Extensions3D*
GraphicsContext3D::getExtensions()
{
    return m_private->extensions();
}

bool
Extensions3DWKC::supports(const String& name)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    return wkcGLExtSupportsPeer(m_peer, name.utf8().data());
}

void
Extensions3DWKC::ensureEnabled(const String& name)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    wkcGLExtEnsureEnabledPeer(m_peer, name.utf8().data());
}

bool
Extensions3DWKC::isEnabled(const String& name)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    return wkcGLExtIsEnabledPeer(m_peer, name.utf8().data());
}

int
Extensions3DWKC::getGraphicsResetStatusARB()
{
    wkcGLMakeContextCurrentPeer(m_peer);
    return wkcGLExtGetGraphicsResetStatusARBPeer(m_peer);
}

void
Extensions3DWKC::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    wkcGLExtBlitFramebufferPeer(m_peer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void
Extensions3DWKC::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    wkcGLExtRenderbufferStorageMultisamplePeer(m_peer, target, samples, internalformat, width, height);
}

Platform3DObject
Extensions3DWKC::createVertexArrayOES()
{
    wkcGLMakeContextCurrentPeer(m_peer);
    return wkcGLExtCreateVertexArrayOESPeer(m_peer);
}

void
Extensions3DWKC::deleteVertexArrayOES(Platform3DObject obj)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    wkcGLExtDeleteVertexArrayOESPeer(m_peer, obj);
}

GC3Dboolean
Extensions3DWKC::isVertexArrayOES(Platform3DObject obj)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    return wkcGLExtIsVertexArrayOESPeer(m_peer, obj);
}

void
Extensions3DWKC::bindVertexArrayOES(Platform3DObject obj)
{
    wkcGLMakeContextCurrentPeer(m_peer);
    wkcGLExtBindVertexArrayOESPeer(m_peer, obj);
}

String
Extensions3DWKC::getTranslatedShaderSourceANGLE(Platform3DObject)
{
    notImplemented();
    return String();
}

IntSize
GraphicsContext3D::getInternalFramebufferSize() const
{
    WKCSize s = {0};
    wkcGLGetInternalFramebufferSizePeer(peer(), &s);
    return s;
}

bool
GraphicsContext3D::paintsToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight,
                                    int canvasWidth, int canvasHeight, void* context) const
{
    return wkcGLPaintsIntoCanvasBufferPeer(peer());
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer*
GraphicsContext3D::platformLayer() const
{
    if (!m_private->layer())
        return 0;
    wkcLayerSetTexturePeer(m_private->layer(), wkcGLDestTexturePeer(peer()));
    return (PlatformLayer *)m_private->layer();
}
#endif

void
GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context, DrawingBuffer* buffer)
{
    if (!context || !context->canvas())
        return;

    ImageBuffer* buf = context->canvas()->buffer();
    if (!buf)
        return;
    GraphicsContext* dc = buf->context();

    int fmt = 0;
    int rowbytes = 0;
    void* mask = 0;
    int maskrowbytes = 0;
    WKCSize wsize = {0};
    void* bitmap = wkcGLLockImagePeer(peer(), &fmt, &rowbytes, &mask, &maskrowbytes, &wsize);

    if (!bitmap) return;

    int wtype = ImageWKC::EColors;
    switch (fmt&WKC_IMAGETYPE_TYPEMASK) {
    case WKC_IMAGETYPE_ARGB8888:
        wtype = ImageWKC::EColorARGB8888;
        break;
    case WKC_IMAGETYPE_RGAB5515:
        wtype = ImageWKC::EColorRGAB5515;
        break;
    case WKC_IMAGETYPE_RGAB5515MASK:
        wtype = ImageWKC::EColorRGAB5515MASK;
        break;
    default:
        break;
    }

    if (wtype!=ImageWKC::EColors) {
        const IntSize size(wsize.fWidth, wsize.fHeight);
        ImageContainerWKC wimg(wtype, bitmap, rowbytes, mask, maskrowbytes, size);
        wimg.setHasAlpha(false);
        wimg.setHasTrueAlpha(false);
        if (fmt&WKC_IMAGETYPE_FLAG_HASALPHA)
            wimg.setHasAlpha(true);
        if (fmt&WKC_IMAGETYPE_FLAG_HASTRUEALPHA)
            wimg.setHasTrueAlpha(true);
        if (m_private->attrs().premultipliedAlpha) {
            wimg.setPremultiplyAlpha(true);
        } else {
            wimg.setPremultiplyAlpha(false);
        }
        RefPtr<BitmapImage> img = BitmapImage::create(reinterpret_cast<NativeImagePtr>(&wimg));
        dc->drawImage(img.get(), WebCore::ColorSpaceDeviceRGB, IntPoint());
    }

    wkcGLUnlockImagePeer(peer(), bitmap);
}

bool
GraphicsContext3D::paintCompositedResultsToCanvas(CanvasRenderingContext*)
{
    notImplemented();
    return false;
}

PassRefPtr<ImageData>
GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer*)
{
    int fmt = 0;
    int rowbytes = 0;
    void* mask = 0;
    int maskrowbytes = 0;
    WKCSize wsize = {0};
    void* bitmap = wkcGLLockImagePeer(peer(), &fmt, &rowbytes, &mask, &maskrowbytes, &wsize);

    if (!bitmap) {
        wkcMemoryNotifyNoMemoryPeer(wsize.fWidth*wsize.fHeight);
        return 0;
    }

    RefPtr<ImageData> img = ImageData::create(IntSize(wsize.fWidth, wsize.fHeight));
    if (!img) {
        wkcMemoryNotifyNoMemoryPeer(wsize.fWidth*wsize.fHeight);
        return 0;
    }
    unsigned char* dest = img->data()->data();

    int r=0, g=0, b=0, a=0;
    const unsigned char* src = 0;
    for (int y=0; y<wsize.fHeight; y++) {
        for (int x=0; x<wsize.fWidth; x++) {
            switch (fmt&WKC_IMAGETYPE_TYPEMASK) {
            case WKC_IMAGETYPE_ARGB8888:
            {
                src = (const unsigned char *)bitmap + y*rowbytes + x*4;
                a = src[0];
                r = src[1];
                g = src[2];
                b = src[3];
                break;
            }
            case WKC_IMAGETYPE_RGAB5515:
            {
                src = (const unsigned char *)bitmap + y*rowbytes + x*2;
                unsigned short v = src[0] + ((int)src[1]<<8);
                if (!(v&0x0020)) {
                    a = 255;
                    r = (((v>>8)&0xf8) | ((v>>11)&0x07));
                    g = (((v>>3)&0xf8) | ((v>>6)&0x07));
                    b = (((v<<3)&0xf8) | (v&0x07));
                } else {
                    a = r = g = b = 0;
                }
                break;
            }
            case WKC_IMAGETYPE_RGAB5515MASK:
            {
                src = (const unsigned char *)bitmap + y*rowbytes + x*2;
                const unsigned char* msrc = (const unsigned char *)mask + y*maskrowbytes + x;
                unsigned short v = src[0] + ((int)src[1]<<8);
                if (!(v&0x0020)) {
                    a = *msrc;
                    r = (((v>>8)&0xf8) | ((v>>11)&0x07));
                    g = (((v>>3)&0xf8) | ((v>>6)&0x07));
                    b = (((v<<3)&0xf8) | (v&0x07));
                } else {
                    a = r = g = b = 0;
                }
                break;
            }
            default:
                break;
            }
        }
        *dest++ = r;
        *dest++ = g;
        *dest++ = b;
        *dest++ = a;
    }

    wkcGLUnlockImagePeer(peer(), bitmap);
    return img;
}

bool
GraphicsContext3D::getImageData(Image* image,
                                GC3Denum format,
                                GC3Denum type,
                                bool premultiplyAlpha,
                                bool ignoreGammaAndColorProfile,
                                Vector<uint8_t>& outputVector)
{
    UNUSED_PARAM(ignoreGammaAndColorProfile);

    if (!image) return false;
    ImageWKC* img = (ImageWKC *)image->nativeImageForCurrentFrame();
    if (!img) return false;

    SourceDataFormat stype = SourceFormatBGRA8;

    const int w = img->size().width();
    const int h = img->size().height();
    void* bitmap = img->bitmap();
    void* mask = img->mask();
    int rb = img->rowbytes();
    int mrb = img->maskrowbytes();
    void* buf = 0;
    bool allocated = false;

    switch (img->type()) {
    case ImageWKC::EColorARGB8888:
        buf = bitmap;
        stype = SourceFormatBGRA8;
        break;

    case ImageWKC::EColorRGAB5515:
    {
        WTF::TryMallocReturnValue r = WTF::tryFastMalloc(w * h * sizeof(unsigned short));
        allocated = r.getValue(buf);
        if (!allocated)
            return false;
        unsigned short* d = reinterpret_cast<unsigned short*>(buf);
        if (img->hasAlpha()) {
            // 5515 -> 5551
            for (int y=0; y<h; y++) {
                const unsigned short* s = reinterpret_cast<unsigned short*>((char *)bitmap + rb*y);
                for (int x=0; x<w; x++) {
                    unsigned short v = *s++;
                    d[x] = (v&0xffc0) | ((v&0x001f)<<1) | ((v&0x0020)>>5);
                }
                d+=w;
            }
            stype = SourceFormatRGBA5551;
        } else {
            // 5505 -> 565
            for (int y=0; y<h; y++) {
                const unsigned short* s = reinterpret_cast<unsigned short*>((char *)bitmap + rb*y);
                for (int x=0; x<w; x++) {
                    unsigned short v = *s++;
                    d[x] = v|0x0020;
                }
                d+=w;
            }
            stype = SourceFormatRGB565;
        }
        break;
    }

    case ImageWKC::EColorRGAB5515MASK:
    {
        // 5515MASK -> 8888
        WTF::TryMallocReturnValue r = WTF::tryFastMalloc(w * h * sizeof(unsigned int));
        allocated = r.getValue(buf);
        if (!allocated)
            return false;
        unsigned int* d = reinterpret_cast<unsigned int*>(buf);
        for (int y=0; y<h; y++) {
            const unsigned short* s = reinterpret_cast<unsigned short*>((char *)bitmap + rb*y);
            const unsigned char* m = reinterpret_cast<unsigned char*>((char *)mask + mrb*y);
            for (int x=0; x<w; x++) {
                unsigned short v = *s++;
                int r = ((v>>8)&0xf8) | ((v>>11)&0x07);
                int g = ((v>>3)&0xf8) | ((v>>6)&0x07);
                int b = ((v<<3)&0xf8) | (v&0x07);
                d[x] = ((*m++)<<24) | (r<<16) | (g<<8) | b;
            }
            d+=w;
        }
        stype = SourceFormatBGRA8;
        break;
    }

    default:
        return false;
    }

    AlphaOp neededAlphaOp = AlphaDoNothing;
    if (premultiplyAlpha)
        neededAlphaOp = AlphaDoPremultiply;
    int len = w * h * 4;
    outputVector.resize(len);
    bool ret = packPixels(static_cast<const uint8_t*>(buf), stype, w, h, 0, format, type, neededAlphaOp, outputVector.data());

    if (allocated) {
        WTF::fastFree(buf);
    }
    return ret;
}

void GraphicsContext3D::getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision)
{
    UNUSED_PARAM(shaderType);
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();

    switch (precisionType) {
    case GraphicsContext3D::LOW_INT:
    case GraphicsContext3D::MEDIUM_INT:
    case GraphicsContext3D::HIGH_INT:
        // These values are for a 32-bit twos-complement integer format.
        range[0] = 31;
        range[1] = 30;
        precision[0] = 0;
        break;
    case GraphicsContext3D::LOW_FLOAT:
    case GraphicsContext3D::MEDIUM_FLOAT:
    case GraphicsContext3D::HIGH_FLOAT:
        // These values are for an IEEE single-precision floating-point format.
        range[0] = 127;
        range[1] = 127;
        precision[0] = 23;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace

#else

#include "GraphicsContext3D.h"

namespace WebCore {

class GraphicsContext3DPrivate
{
public:
    static PassOwnPtr<GraphicsContext3DPrivate> create(GraphicsContext3D::Attributes& attrs, HostWindow* hostwindow, bool in_direct)
    {
        return nullptr;
    }
    ~GraphicsContext3DPrivate() {}

private:
    GraphicsContext3DPrivate() {}
};

GraphicsContext3D::~GraphicsContext3D()
{
}

Platform3DObject
GraphicsContext3D::createFramebuffer()
{
    return 0;
}

void
GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
}

GC3Dint
GraphicsContext3D::getUniformLocation(Platform3DObject obj, const String& name)
{
    return 0;
}

} // namespace

#endif // ENABLE(WEBGL)
