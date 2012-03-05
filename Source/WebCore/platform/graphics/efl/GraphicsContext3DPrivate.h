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

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include "GraphicsContext3D.h"

#include <Evas_GL.h>

namespace WebCore {

class PageClientEfl;

class GraphicsContext3DPrivate {
public:
    static PassOwnPtr<GraphicsContext3DPrivate> create(GraphicsContext3D::Attributes attrs, HostWindow*, bool renderDirectlyToEvasGLObject);
    ~GraphicsContext3DPrivate();

    PlatformGraphicsContext3D platformGraphicsContext3D() const;

#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer() const;
#endif

    bool makeContextCurrent();

    bool isGLES2Compliant() const;

    void activeTexture(GC3Denum texture);
    void attachShader(Platform3DObject program, Platform3DObject shader);
    void bindAttribLocation(Platform3DObject, GC3Duint index, const String& name);
    void bindBuffer(GC3Denum target, Platform3DObject);
    void bindFramebuffer(GC3Denum target, Platform3DObject);
    void bindRenderbuffer(GC3Denum target, Platform3DObject);
    void bindTexture(GC3Denum target, Platform3DObject);
    void blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha);
    void blendEquation(GC3Denum mode);
    void blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha);
    void blendFunc(GC3Denum srcFactor, GC3Denum dstFactor);
    void blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha);

    void bufferData(GC3Denum target, GC3Dsizeiptr, const void* data, GC3Denum usage);
    void bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr, const void* data);

    GC3Denum checkFramebufferStatus(GC3Denum target);
    void clear(GC3Dbitfield mask);
    void clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha);
    void clearDepth(GC3Dclampf depth);
    void clearStencil(GC3Dint clearValue);
    void colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha);
    void compileShader(Platform3DObject);

    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border);
    void copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void cullFace(GC3Denum mode);
    void depthFunc(GC3Denum func);
    void depthMask(GC3Dboolean flag);
    void depthRange(GC3Dclampf zNear, GC3Dclampf zFar);
    void detachShader(Platform3DObject, Platform3DObject);
    void disable(GC3Denum cap);
    void disableVertexAttribArray(GC3Duint index);
    void drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count);
    void drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset);

    void enable(GC3Denum cap);
    void enableVertexAttribArray(GC3Duint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbufferTarget, Platform3DObject);
    void framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum texTarget, Platform3DObject, GC3Dint level);
    void frontFace(GC3Denum mode);
    void generateMipmap(GC3Denum target);

    bool getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo&);
    bool getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo&);
    void getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders);
    GC3Dint getAttribLocation(Platform3DObject, const String& name);
    void getBooleanv(GC3Denum paramName, GC3Dboolean* value);
    void getBufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value);
    GraphicsContext3D::Attributes getContextAttributes();
    GC3Denum getError();
    void getFloatv(GC3Denum paramName, GC3Dfloat* value);
    void getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum paramName, GC3Dint* value);
    void getIntegerv(GC3Denum paramName, GC3Dint* value);
    void getProgramiv(Platform3DObject program, GC3Denum paramName, GC3Dint* value);
    String getProgramInfoLog(Platform3DObject);
    void getRenderbufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value);
    void getShaderiv(Platform3DObject, GC3Denum paramName, GC3Dint* value);
    String getShaderInfoLog(Platform3DObject);

    String getShaderSource(Platform3DObject);
    String getString(GC3Denum name);
    void getTexParameterfv(GC3Denum target, GC3Denum paramName, GC3Dfloat* value);
    void getTexParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value);
    void getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value);
    void getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value);
    GC3Dint getUniformLocation(Platform3DObject, const String& name);
    void getVertexAttribfv(GC3Duint index, GC3Denum paramName, GC3Dfloat* value);
    void getVertexAttribiv(GC3Duint index, GC3Denum paramName, GC3Dint* value);
    GC3Dsizeiptr getVertexAttribOffset(GC3Duint index, GC3Denum paramName);

    void hint(GC3Denum target, GC3Denum mode);
    GC3Dboolean isBuffer(Platform3DObject);
    GC3Dboolean isEnabled(GC3Denum cap);
    GC3Dboolean isFramebuffer(Platform3DObject);
    GC3Dboolean isProgram(Platform3DObject);
    GC3Dboolean isRenderbuffer(Platform3DObject);
    GC3Dboolean isShader(Platform3DObject);
    GC3Dboolean isTexture(Platform3DObject);
    void lineWidth(GC3Dfloat);
    void linkProgram(Platform3DObject);
    void pixelStorei(GC3Denum paramName, GC3Dint param);
    void polygonOffset(GC3Dfloat factor, GC3Dfloat units);

    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data);

    void renderbufferStorage(GC3Denum target, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height);
    void sampleCoverage(GC3Dclampf value, GC3Dboolean invert);
    void scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void shaderSource(Platform3DObject, const String&);
    void stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilMask(GC3Duint mask);
    void stencilMaskSeparate(GC3Denum face, GC3Duint mask);
    void stencilOp(GC3Denum fail, GC3Denum zFail, GC3Denum zPass);
    void stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zFail, GC3Denum zPass);

    bool texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels);
    void texParameterf(GC3Denum target, GC3Denum paramName, GC3Dfloat param);
    void texParameteri(GC3Denum target, GC3Denum paramName, GC3Dint param);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xOffset, GC3Dint yOffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels);

    void uniform1f(GC3Dint location, GC3Dfloat x);
    void uniform1fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform1i(GC3Dint location, GC3Dint x);
    void uniform1iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniform2f(GC3Dint location, GC3Dfloat x, float y);
    void uniform2fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform2i(GC3Dint location, GC3Dint x, GC3Dint y);
    void uniform2iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void uniform3fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z);
    void uniform3iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void uniform4fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w);
    void uniform4iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniformMatrix2fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, GC3Dfloat* value);
    void uniformMatrix3fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, GC3Dfloat* value);
    void uniformMatrix4fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, GC3Dfloat* value);

    void useProgram(Platform3DObject);
    void validateProgram(Platform3DObject);

    void vertexAttrib1f(GC3Duint index, GC3Dfloat x);
    void vertexAttrib1fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y);
    void vertexAttrib2fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void vertexAttrib3fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void vertexAttrib4fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized,
                             GC3Dsizei stride, GC3Dintptr offset);

    void viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    Platform3DObject createBuffer();
    Platform3DObject createFramebuffer();
    Platform3DObject createProgram();
    Platform3DObject createRenderbuffer();
    Platform3DObject createShader(GC3Denum);
    Platform3DObject createTexture();

    void deleteBuffer(Platform3DObject);
    void deleteFramebuffer(Platform3DObject);
    void deleteProgram(Platform3DObject);
    void deleteRenderbuffer(Platform3DObject);
    void deleteShader(Platform3DObject);
    void deleteTexture(Platform3DObject);

    void synthesizeGLError(GC3Denum error);

    Extensions3D* getExtensions();

private:
    GraphicsContext3DPrivate();

    bool initialize(GraphicsContext3D::Attributes attrs, HostWindow*, bool renderDirectlyToHostWindow);

    bool createSurface(PageClientEfl*, bool renderDirectlyToEvasGLObject);

    GraphicsContext3D::Attributes m_attributes;

    Platform3DObject m_boundFBO;
    Platform3DObject m_boundTexture;
    Platform3DObject m_boundArrayBuffer;

    ListHashSet<GC3Denum> m_syntheticErrors;

    Evas_GL* m_evasGL;
    Evas_GL_Context* m_context;
    Evas_GL_Surface* m_surface;
    Evas_GL_API* m_api;
};

} // namespace WebCore

#endif // GraphicsLayerEfl_h
