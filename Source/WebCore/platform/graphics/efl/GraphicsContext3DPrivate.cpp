/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(WEBGL) || USE(ACCELERATED_COMPOSITING)

#include "GraphicsContext3DPrivate.h"

#include "HostWindow.h"
#include "NotImplemented.h"
#include "PageClientEfl.h"

#include <wtf/OwnArrayPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

PassOwnPtr<GraphicsContext3DPrivate> GraphicsContext3DPrivate::create(GraphicsContext3D::Attributes attributes, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
{
    OwnPtr<GraphicsContext3DPrivate> internal = adoptPtr(new GraphicsContext3DPrivate());

    if (!internal->initialize(attributes, hostWindow, renderDirectlyToHostWindow))
        return nullptr;

    return internal.release();
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate()
    : m_boundFBO(0)
    , m_boundTexture(0)
    , m_boundArrayBuffer(0)
    , m_evasGL(0)
    , m_context(0)
    , m_surface(0)
    , m_api(0)
{
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
    if (!m_evasGL)
        return;

    if (m_surface)
        evas_gl_surface_destroy(m_evasGL, m_surface);

    if (m_context)
        evas_gl_context_destroy(m_evasGL, m_context);

    evas_gl_free(m_evasGL);
}

bool GraphicsContext3DPrivate::initialize(GraphicsContext3D::Attributes attributes, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
{
    PageClientEfl* pageClient = static_cast<PageClientEfl*>(hostWindow->platformPageClient());

    Evas* evas = evas_object_evas_get(pageClient->view());

    // Create a new Evas_GL object for gl rendering on efl.
    m_evasGL = evas_gl_new(evas);
    if (!m_evasGL)
        return false;

    // Get the API for rendering using OpenGL.
    // This returns a structure that contains all the OpenGL functions we can use to render in Evas
    m_api = evas_gl_api_get(m_evasGL);
    if (!m_api)
        return false;

    Evas_GL_Context* shareContext = 0;

#if USE(ACCELERATED_COMPOSITING)
    // GC3D with RenderOffscreen style for WebGL has to be shared with AC's context when AC is enabled.
    if (!renderDirectlyToHostWindow) {
        GraphicsContext3D* context = pageClient->acceleratedCompositingContext();
        if (context)
            shareContext = static_cast<Evas_GL_Context*>(context->platformGraphicsContext3D());
    }
#endif

    // Create a context
    m_context = evas_gl_context_create(m_evasGL, shareContext);
    if (!m_context)
        return false;

    // Create a surface
    if (!createSurface(pageClient, renderDirectlyToHostWindow))
        return false;

    return makeContextCurrent();
}

bool GraphicsContext3DPrivate::createSurface(PageClientEfl* pageClient, bool renderDirectlyToHostWindow)
{
    // If RenderStyle is RenderOffscreen, we will be rendering to a FBO,
    // so Evas_GL_Surface has a 1x1 dummy surface.
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;

    // But, in case of RenderDirectlyToHostWindow, we have to render to a render target surface with the same size as our webView.
    if (renderDirectlyToHostWindow)
        evas_object_geometry_get(pageClient->view(), &x, &y, &width, &height);

    Evas_GL_Config config = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE, // FIXME: set EVAS_GL_STENCIL_BIT_8 after fixing Evas_GL bug.
        EVAS_GL_OPTIONS_NONE
    };

    // Create a new Evas_GL_Surface object
    m_surface = evas_gl_surface_create(m_evasGL, &config, width, height);
    if (!m_surface)
        return false;

#if USE(ACCELERATED_COMPOSITING)
    if (renderDirectlyToHostWindow) {
        Evas_Native_Surface nativeSurface;
        // Fill in the Native Surface information from the given Evas GL surface.
        evas_gl_native_surface_get(m_evasGL, m_surface, &nativeSurface);

        // Create and specially set up a evas_object which act as the render targer surface.
        if (!pageClient->createEvasObjectForAcceleratedCompositing(&nativeSurface, IntRect(x, y, width, height)))
            return false;
    }
#endif
    return true;
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformGraphicsContext3D() const
{
    return m_context;
}

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    return evas_gl_make_current(m_evasGL, m_surface, m_context);
}

bool GraphicsContext3DPrivate::isGLES2Compliant() const
{
    return true;
}

void GraphicsContext3DPrivate::activeTexture(GC3Denum texture)
{
    makeContextCurrent();
    m_api->glActiveTexture(texture);
}

void GraphicsContext3DPrivate::attachShader(Platform3DObject program, Platform3DObject shader)
{
    makeContextCurrent();
    m_api->glAttachShader(program, shader);
}

void GraphicsContext3DPrivate::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    makeContextCurrent();
    m_api->glBindAttribLocation(program, index, name.utf8().data());
}

void GraphicsContext3DPrivate::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    m_api->glBindBuffer(target, buffer);

    if (target == GL_ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
}

void GraphicsContext3DPrivate::bindFramebuffer(GC3Denum target, Platform3DObject framebuffer)
{
    makeContextCurrent();

    if (framebuffer != m_boundFBO) {
        m_api->glBindFramebuffer(target, framebuffer);
        m_boundFBO = framebuffer;
    }
}

void GraphicsContext3DPrivate::bindRenderbuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    m_api->glBindRenderbuffer(target, buffer);
}

void GraphicsContext3DPrivate::bindTexture(GC3Denum target, Platform3DObject texture)
{
    makeContextCurrent();
    m_api->glBindTexture(target, texture);
    m_boundTexture = texture;
}

void GraphicsContext3DPrivate::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    m_api->glBlendColor(red, green, blue, alpha);
}

void GraphicsContext3DPrivate::blendEquation(GC3Denum mode)
{
    makeContextCurrent();
    m_api->glBlendEquation(mode);
}

void GraphicsContext3DPrivate::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    makeContextCurrent();
    m_api->glBlendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3DPrivate::blendFunc(GC3Denum srcFactor, GC3Denum dstFactor)
{
    makeContextCurrent();
    m_api->glBlendFunc(srcFactor, dstFactor);
}

void GraphicsContext3DPrivate::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    makeContextCurrent();
    m_api->glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3DPrivate::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    makeContextCurrent();
    m_api->glBufferData(target, size, data, usage);
}

void GraphicsContext3DPrivate::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    makeContextCurrent();
    m_api->glBufferSubData(target, offset, size, data);
}

GC3Denum GraphicsContext3DPrivate::checkFramebufferStatus(GC3Denum target)
{
    makeContextCurrent();
    return m_api->glCheckFramebufferStatus(target);
}

void GraphicsContext3DPrivate::clear(GC3Dbitfield mask)
{
    makeContextCurrent();
    m_api->glClear(mask);
}

void GraphicsContext3DPrivate::clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    m_api->glClearColor(red, green, blue, alpha);
}

void GraphicsContext3DPrivate::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
    m_api->glClearDepthf(depth);
}

void GraphicsContext3DPrivate::clearStencil(GC3Dint clearValue)
{
    makeContextCurrent();
    m_api->glClearStencil(clearValue);
}

void GraphicsContext3DPrivate::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    makeContextCurrent();
    m_api->glColorMask(red, green, blue, alpha);
}

void GraphicsContext3DPrivate::compileShader(Platform3DObject shader)
{
    makeContextCurrent();
    m_api->glCompileShader(shader);
}

void GraphicsContext3DPrivate::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    makeContextCurrent();
    m_api->glCopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

void GraphicsContext3DPrivate::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    m_api->glCopyTexSubImage2D(target, level, xOffset, yOffset, x, y, width, height);
}

void GraphicsContext3DPrivate::cullFace(GC3Denum mode)
{
    makeContextCurrent();
    m_api->glCullFace(mode);
}

void GraphicsContext3DPrivate::depthFunc(GC3Denum func)
{
    makeContextCurrent();
    m_api->glDepthFunc(func);
}

void GraphicsContext3DPrivate::depthMask(GC3Dboolean flag)
{
    makeContextCurrent();
    m_api->glDepthMask(flag);
}

void GraphicsContext3DPrivate::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
    m_api->glDepthRangef(zNear, zFar);
}

void GraphicsContext3DPrivate::detachShader(Platform3DObject program, Platform3DObject shader)
{
    makeContextCurrent();
    m_api->glDetachShader(program, shader);
}

void GraphicsContext3DPrivate::disable(GC3Denum cap)
{
    makeContextCurrent();
    m_api->glDisable(cap);
}

void GraphicsContext3DPrivate::disableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    m_api->glDisableVertexAttribArray(index);
}

void GraphicsContext3DPrivate::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    makeContextCurrent();
    m_api->glDrawArrays(mode, first, count);
}

void GraphicsContext3DPrivate::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    makeContextCurrent();
    m_api->glDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3DPrivate::enable(GC3Denum cap)
{
    makeContextCurrent();
    m_api->glEnable(cap);
}

void GraphicsContext3DPrivate::enableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    m_api->glEnableVertexAttribArray(index);
}

void GraphicsContext3DPrivate::finish()
{
    makeContextCurrent();
    m_api->glFinish();
}

void GraphicsContext3DPrivate::flush()
{
    makeContextCurrent();
    m_api->glFlush();
}

void GraphicsContext3DPrivate::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbufferTarget, Platform3DObject renderbuffer)
{
    makeContextCurrent();
    m_api->glFramebufferRenderbuffer(target, attachment, renderbufferTarget, renderbuffer);
}

void GraphicsContext3DPrivate::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum texTarget, Platform3DObject texture, GC3Dint level)
{
    makeContextCurrent();
    m_api->glFramebufferTexture2D(target, attachment, texTarget, texture, level);
}

void GraphicsContext3DPrivate::frontFace(GC3Denum mode)
{
    makeContextCurrent();
    m_api->glFrontFace(mode);
}

void GraphicsContext3DPrivate::generateMipmap(GC3Denum target)
{
    makeContextCurrent();
    m_api->glGenerateMipmap(target);
}

bool GraphicsContext3DPrivate::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(GL_INVALID_VALUE);
        return false;
    }

    makeContextCurrent();

    GLint maxNameLength = 0;
    m_api->glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxNameLength);
    if (!maxNameLength)
        return false;

    OwnArrayPtr<char> name = adoptArrayPtr(new char[maxNameLength]);
    if (!name) {
        synthesizeGLError(GL_OUT_OF_MEMORY);
        return false;
    }

    GLsizei length = 0;
    GLint size = 0;
    GLenum type = 0;
    m_api->glGetActiveAttrib(program, index, maxNameLength, &length, &size, &type, name.get());
    if (!length)
        return false;

    info.name = String::fromUTF8(name.get(), length);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3DPrivate::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(GL_INVALID_VALUE);
        return false;
    }

    makeContextCurrent();

    GLint maxNameLength = 0;
    m_api->glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
    if (!maxNameLength)
        return false;

    OwnArrayPtr<char> name = adoptArrayPtr(new char[maxNameLength]);
    if (!name) {
        synthesizeGLError(GL_OUT_OF_MEMORY);
        return false;
    }

    GLsizei length = 0;
    GLint size = 0;
    GLenum type = 0;
    m_api->glGetActiveUniform(program, index, maxNameLength, &length, &size, &type, name.get());
    if (!length)
        return false;

    info.name = String::fromUTF8(name.get(), length);
    info.type = type;
    info.size = size;
    return true;
}

void GraphicsContext3DPrivate::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
{
    makeContextCurrent();
    m_api->glGetAttachedShaders(program, maxCount, count, shaders);
}

int GraphicsContext3DPrivate::getAttribLocation(Platform3DObject program, const String& name)
{
    makeContextCurrent();
    return m_api->glGetAttribLocation(program, name.utf8().data());
}

void GraphicsContext3DPrivate::getBooleanv(GC3Denum paramName, GC3Dboolean* value)
{
    makeContextCurrent();
    m_api->glGetBooleanv(paramName, value);
}

void GraphicsContext3DPrivate::getBufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetBufferParameteriv(target, paramName, value);
}

GraphicsContext3D::Attributes GraphicsContext3DPrivate::getContextAttributes()
{
    return m_attributes;
}

GC3Denum GraphicsContext3DPrivate::getError()
{
    if (!m_syntheticErrors.isEmpty()) {
        GC3Denum error = m_syntheticErrors.first();
        m_syntheticErrors.remove(m_syntheticErrors.begin());
        return error;
    }

    makeContextCurrent();
    return m_api->glGetError();
}

void GraphicsContext3DPrivate::getFloatv(GC3Denum paramName, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glGetFloatv(paramName, value);
}

void GraphicsContext3DPrivate::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetFramebufferAttachmentParameteriv(target, attachment, paramName, value);
}

void GraphicsContext3DPrivate::getIntegerv(GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetIntegerv(paramName, value);
}

void GraphicsContext3DPrivate::getProgramiv(Platform3DObject program, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetProgramiv(program, paramName, value);
}

String GraphicsContext3DPrivate::getProgramInfoLog(Platform3DObject program)
{
    makeContextCurrent();

    GLint logLength = 0;
    m_api->glGetProgramiv(program, GraphicsContext3D::INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return String();

    OwnArrayPtr<char> log = adoptArrayPtr(new char[logLength]);
    if (!log)
        return String();

    GLint returnedLogLength = 0;
    m_api->glGetProgramInfoLog(program, logLength, &returnedLogLength, log.get());
    ASSERT(logLength == returnedLogLength + 1);

    String result = String::fromUTF8(log.get(), returnedLogLength);
    return result;
}

void GraphicsContext3DPrivate::getRenderbufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetRenderbufferParameteriv(target, paramName, value);
}

void GraphicsContext3DPrivate::getShaderiv(Platform3DObject shader, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetShaderiv(shader, paramName, value);
}

String GraphicsContext3DPrivate::getShaderInfoLog(Platform3DObject shader)
{
    makeContextCurrent();

    GLint logLength = 0;
    m_api->glGetShaderiv(shader, GraphicsContext3D::INFO_LOG_LENGTH, &logLength);
    if (logLength <= 1)
        return String();

    OwnArrayPtr<char> log = adoptArrayPtr(new char[logLength]);
    if (!log)
        return String();

    GLint returnedLogLength = 0;
    m_api->glGetShaderInfoLog(shader, logLength, &returnedLogLength, log.get());
    ASSERT(logLength == returnedLogLength + 1);

    String result = String::fromUTF8(log.get(), returnedLogLength);
    return result;
}

String GraphicsContext3DPrivate::getShaderSource(Platform3DObject shader)
{
    makeContextCurrent();

    GLint logLength = 0;
    m_api->glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
    if (logLength <= 1)
        return String();

    OwnArrayPtr<char> log = adoptArrayPtr(new char[logLength]);
    if (!log)
        return String();

    GLint returnedLogLength = 0;
    m_api->glGetShaderSource(shader, logLength, &returnedLogLength, log.get());
    ASSERT(logLength == returnedLogLength + 1);

    String result = String::fromUTF8(log.get(), returnedLogLength);
    return result;
}

String GraphicsContext3DPrivate::getString(GC3Denum name)
{
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(m_api->glGetString(name)));
}

void GraphicsContext3DPrivate::getTexParameterfv(GC3Denum target, GC3Denum paramName, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glGetTexParameterfv(target, paramName, value);
}

void GraphicsContext3DPrivate::getTexParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetTexParameteriv(target, paramName, value);
}

void GraphicsContext3DPrivate::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glGetUniformfv(program, location, value);
}

void GraphicsContext3DPrivate::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetUniformiv(program, location, value);
}

GC3Dint GraphicsContext3DPrivate::getUniformLocation(Platform3DObject program, const String& name)
{
    makeContextCurrent();
    return m_api->glGetUniformLocation(program, name.utf8().data());
}

void GraphicsContext3DPrivate::getVertexAttribfv(GC3Duint index, GC3Denum paramName, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glGetVertexAttribfv(index, paramName, value);
}

void GraphicsContext3DPrivate::getVertexAttribiv(GC3Duint index, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    m_api->glGetVertexAttribiv(index, paramName, value);
}

GC3Dsizeiptr GraphicsContext3DPrivate::getVertexAttribOffset(GC3Duint index, GC3Denum paramName)
{
    makeContextCurrent();
    void* pointer = 0;
    m_api->glGetVertexAttribPointerv(index, paramName, &pointer);
    return reinterpret_cast<GC3Dsizeiptr>(pointer);
}

void GraphicsContext3DPrivate::hint(GC3Denum target, GC3Denum mode)
{
    makeContextCurrent();
    m_api->glHint(target, mode);
}

GC3Dboolean GraphicsContext3DPrivate::isBuffer(Platform3DObject buffer)
{
    makeContextCurrent();
    return m_api->glIsBuffer(buffer);
}

GC3Dboolean GraphicsContext3DPrivate::isEnabled(GC3Denum cap)
{
    makeContextCurrent();
    return m_api->glIsEnabled(cap);
}

GC3Dboolean GraphicsContext3DPrivate::isFramebuffer(Platform3DObject framebuffer)
{
    makeContextCurrent();
    return m_api->glIsFramebuffer(framebuffer);
}

GC3Dboolean GraphicsContext3DPrivate::isProgram(Platform3DObject program)
{
    makeContextCurrent();
    return m_api->glIsProgram(program);
}

GC3Dboolean GraphicsContext3DPrivate::isRenderbuffer(Platform3DObject renderbuffer)
{
    makeContextCurrent();
    return m_api->glIsRenderbuffer(renderbuffer);
}

GC3Dboolean GraphicsContext3DPrivate::isShader(Platform3DObject shader)
{
    makeContextCurrent();
    return m_api->glIsShader(shader);
}

GC3Dboolean GraphicsContext3DPrivate::isTexture(Platform3DObject texture)
{
    makeContextCurrent();
    return m_api->glIsTexture(texture);
}

void GraphicsContext3DPrivate::lineWidth(GC3Dfloat width)
{
    makeContextCurrent();
    m_api->glLineWidth(width);
}

void GraphicsContext3DPrivate::linkProgram(Platform3DObject program)
{
    makeContextCurrent();
    m_api->glLinkProgram(program);
}

void GraphicsContext3DPrivate::pixelStorei(GC3Denum paramName, GC3Dint param)
{
    makeContextCurrent();
    m_api->glPixelStorei(paramName, param);
}

void GraphicsContext3DPrivate::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    makeContextCurrent();
    m_api->glPolygonOffset(factor, units);
}

void GraphicsContext3DPrivate::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    makeContextCurrent();

    m_api->glFlush();
    m_api->glReadPixels(x, y, width, height, format, type, data);
}

void GraphicsContext3DPrivate::renderbufferStorage(GC3Denum target, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    m_api->glRenderbufferStorage(target, internalFormat, width, height);
}

void GraphicsContext3DPrivate::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    makeContextCurrent();
    m_api->glSampleCoverage(value, invert);
}

void GraphicsContext3DPrivate::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    m_api->glScissor(x, y, width, height);
}

void GraphicsContext3DPrivate::shaderSource(Platform3DObject shader, const String& string)
{
    makeContextCurrent();
    const char* str = string.utf8().data();
    int length = string.length();
    m_api->glShaderSource(shader, 1, &str, &length);
}

void GraphicsContext3DPrivate::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    m_api->glStencilFunc(func, ref, mask);
}

void GraphicsContext3DPrivate::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    m_api->glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3DPrivate::stencilMask(GC3Duint mask)
{
    makeContextCurrent();
    m_api->glStencilMask(mask);
}

void GraphicsContext3DPrivate::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    makeContextCurrent();
    m_api->glStencilMaskSeparate(face, mask);
}

void GraphicsContext3DPrivate::stencilOp(GC3Denum fail, GC3Denum zFail, GC3Denum zPass)
{
    makeContextCurrent();
    m_api->glStencilOp(fail, zFail, zPass);
}

void GraphicsContext3DPrivate::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zFail, GC3Denum zPass)
{
    makeContextCurrent();
    m_api->glStencilOpSeparate(face, fail, zFail, zPass);
}

bool GraphicsContext3DPrivate::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    m_api->glTexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3DPrivate::texParameterf(GC3Denum target, GC3Denum paramName, GC3Dfloat param)
{
    makeContextCurrent();
    m_api->glTexParameterf(target, paramName, param);
}

void GraphicsContext3DPrivate::texParameteri(GC3Denum target, GC3Denum paramName, GC3Dint param)
{
    makeContextCurrent();
    m_api->glTexParameteri(target, paramName, param);
}

void GraphicsContext3DPrivate::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    m_api->glTexSubImage2D(target, level, xOffset, yOffset, width, height, format, type, pixels);
}

void GraphicsContext3DPrivate::uniform1f(GC3Dint location, GC3Dfloat x)
{
    makeContextCurrent();
    m_api->glUniform1f(location, x);
}

void GraphicsContext3DPrivate::uniform1fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* array)
{
    makeContextCurrent();
    m_api->glUniform1fv(location, size, array);
}

void GraphicsContext3DPrivate::uniform1i(GC3Dint location, GC3Dint x)
{
    makeContextCurrent();
    m_api->glUniform1i(location, x);
}

void GraphicsContext3DPrivate::uniform1iv(GC3Dint location, GC3Dsizei size, GC3Dint* array)
{
    makeContextCurrent();
    m_api->glUniform1iv(location, size, array);
}

void GraphicsContext3DPrivate::uniform2f(GC3Dint location, GC3Dfloat x, GC3Dfloat y)
{
    makeContextCurrent();
    m_api->glUniform2f(location, x, y);
}

void GraphicsContext3DPrivate::uniform2fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* array)
{
    makeContextCurrent();
    m_api->glUniform2fv(location, size, array);
}

void GraphicsContext3DPrivate::uniform2i(GC3Dint location, GC3Dint x, GC3Dint y)
{
    makeContextCurrent();
    m_api->glUniform2i(location, x, y);
}

void GraphicsContext3DPrivate::uniform2iv(GC3Dint location, GC3Dsizei size, GC3Dint* array)
{
    makeContextCurrent();
    m_api->glUniform2iv(location, size, array);
}

void GraphicsContext3DPrivate::uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    makeContextCurrent();
    m_api->glUniform3f(location, x, y, z);
}

void GraphicsContext3DPrivate::uniform3fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* array)
{
    makeContextCurrent();
    m_api->glUniform3fv(location, size, array);
}

void GraphicsContext3DPrivate::uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    makeContextCurrent();
    m_api->glUniform3i(location, x, y, z);
}

void GraphicsContext3DPrivate::uniform3iv(GC3Dint location, GC3Dsizei size, GC3Dint* array)
{
    makeContextCurrent();
    m_api->glUniform3iv(location, size, array);
}

void GraphicsContext3DPrivate::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    makeContextCurrent();
    m_api->glUniform4f(location, x, y, z, w);
}

void GraphicsContext3DPrivate::uniform4fv(GC3Dint location, GC3Dsizei size, GC3Dfloat* array)
{
    makeContextCurrent();
    m_api->glUniform4fv(location, size, array);
}

void GraphicsContext3DPrivate::uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    makeContextCurrent();
    m_api->glUniform4i(location, x, y, z, w);
}

void GraphicsContext3DPrivate::uniform4iv(GC3Dint location, GC3Dsizei size, GC3Dint* array)
{
    makeContextCurrent();
    m_api->glUniform4iv(location, size, array);
}

void GraphicsContext3DPrivate::uniformMatrix2fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glUniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3DPrivate::uniformMatrix3fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glUniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3DPrivate::uniformMatrix4fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, GC3Dfloat* value)
{
    makeContextCurrent();
    m_api->glUniformMatrix4fv(location, size, transpose, value);
}

void GraphicsContext3DPrivate::useProgram(Platform3DObject program)
{
    makeContextCurrent();
    m_api->glUseProgram(program);
}

void GraphicsContext3DPrivate::validateProgram(Platform3DObject program)
{
    makeContextCurrent();
    m_api->glValidateProgram(program);
}

void GraphicsContext3DPrivate::vertexAttrib1f(GC3Duint index, GC3Dfloat x)
{
    makeContextCurrent();
    m_api->glVertexAttrib1f(index, x);
}

void GraphicsContext3DPrivate::vertexAttrib1fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    m_api->glVertexAttrib1fv(index, values);
}

void GraphicsContext3DPrivate::vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y)
{
    makeContextCurrent();
    m_api->glVertexAttrib2f(index, x, y);
}

void GraphicsContext3DPrivate::vertexAttrib2fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    m_api->glVertexAttrib2fv(index, values);
}

void GraphicsContext3DPrivate::vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    makeContextCurrent();
    m_api->glVertexAttrib3f(index, x, y, z);
}

void GraphicsContext3DPrivate::vertexAttrib3fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    m_api->glVertexAttrib3fv(index, values);
}

void GraphicsContext3DPrivate::vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    makeContextCurrent();
    m_api->glVertexAttrib4f(index, x, y, z, w);
}

void GraphicsContext3DPrivate::vertexAttrib4fv(GC3Duint index, GC3Dfloat* values)
{
    makeContextCurrent();
    m_api->glVertexAttrib4fv(index, values);
}

void GraphicsContext3DPrivate::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    makeContextCurrent();

    if (m_boundArrayBuffer <= 0)
        return;

    m_api->glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3DPrivate::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    m_api->glViewport(x, y, width, height);
}

Platform3DObject GraphicsContext3DPrivate::createBuffer()
{
    makeContextCurrent();
    Platform3DObject buffer = 0;
    m_api->glGenBuffers(1, &buffer);
    return buffer;
}

Platform3DObject GraphicsContext3DPrivate::createFramebuffer()
{
    makeContextCurrent();
    Platform3DObject buffer = 0;
    m_api->glGenFramebuffers(1, &buffer);
    return buffer;
}

Platform3DObject GraphicsContext3DPrivate::createProgram()
{
    makeContextCurrent();
    return m_api->glCreateProgram();
}

Platform3DObject GraphicsContext3DPrivate::createRenderbuffer()
{
    makeContextCurrent();
    Platform3DObject buffer;
    m_api->glGenRenderbuffers(1, &buffer);
    return buffer;
}

Platform3DObject GraphicsContext3DPrivate::createShader(GC3Denum shaderType)
{
    makeContextCurrent();
    return m_api->glCreateShader(shaderType);
}

Platform3DObject GraphicsContext3DPrivate::createTexture()
{
    makeContextCurrent();
    Platform3DObject texture;
    m_api->glGenTextures(1, &texture);
    return texture;
}

void GraphicsContext3DPrivate::deleteBuffer(Platform3DObject buffer)
{
    makeContextCurrent();
    m_api->glDeleteBuffers(1, &buffer);
}

void GraphicsContext3DPrivate::deleteFramebuffer(Platform3DObject framebuffer)
{
    makeContextCurrent();
    m_api->glDeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3DPrivate::deleteProgram(Platform3DObject program)
{
    makeContextCurrent();
    m_api->glDeleteProgram(program);
}

void GraphicsContext3DPrivate::deleteRenderbuffer(Platform3DObject renderbuffer)
{
    makeContextCurrent();
    m_api->glDeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3DPrivate::deleteShader(Platform3DObject shader)
{
    makeContextCurrent();
    m_api->glDeleteShader(shader);
}

void GraphicsContext3DPrivate::deleteTexture(Platform3DObject texture)
{
    makeContextCurrent();
    m_api->glDeleteTextures(1, &texture);
}

void GraphicsContext3DPrivate::synthesizeGLError(GC3Denum error)
{
    m_syntheticErrors.add(error);
}

Extensions3D* GraphicsContext3DPrivate::getExtensions()
{
    notImplemented();
    return 0;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) || USE(ACCELERATED_COMPOSITING)
