/*
    Copyright (C) 2010 Tieto Corporation.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
     
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
     
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include "GraphicsContext3D.h"

#include "WebGLObject.h"
#include "CanvasRenderingContext.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "NotImplemented.h"
#include "QWebPageClient.h"
#include <QAbstractScrollArea>
#include <QGLContext>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

#if ENABLE(3D_CANVAS)

namespace WebCore {

#if !defined(GLchar)
typedef char GLchar;
#endif

#if !defined(APIENTRY)
#define APIENTRY
#endif

#ifdef QT_OPENGL_ES_2
typedef GLsizeiptr GLsizeiptrType;
typedef GLintptr GLintptrType;
#else
typedef ptrdiff_t GLsizeiptrType;
typedef ptrdiff_t GLintptrType;
#endif

typedef void (APIENTRY* glActiveTextureType) (GLenum);
typedef void (APIENTRY* glAttachShaderType) (GLuint, GLuint);
typedef void (APIENTRY* glBindAttribLocationType) (GLuint, GLuint, const char*);
typedef void (APIENTRY* glBindBufferType) (GLenum, GLuint);
typedef void (APIENTRY* glBindFramebufferType) (GLenum, GLuint);
typedef void (APIENTRY* glBindRenderbufferType) (GLenum, GLuint);
typedef void (APIENTRY* glBlendColorType) (GLclampf, GLclampf, GLclampf, GLclampf);
typedef void (APIENTRY* glBlendEquationType) (GLenum);
typedef void (APIENTRY* glBlendEquationSeparateType)(GLenum, GLenum);
typedef void (APIENTRY* glBlendFuncSeparateType)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (APIENTRY* glBufferDataType) (GLenum, GLsizeiptrType, const GLvoid*, GLenum);
typedef void (APIENTRY* glBufferSubDataType) (GLenum, GLintptrType, GLsizeiptrType, const GLvoid*);
typedef GLenum (APIENTRY* glCheckFramebufferStatusType) (GLenum);
typedef void (APIENTRY* glCompileShaderType) (GLuint);
typedef GLuint (APIENTRY* glCreateProgramType) ();
typedef GLuint (APIENTRY* glCreateShaderType) (GLenum);
typedef void (APIENTRY* glDeleteBuffersType) (GLsizei, const GLuint*);
typedef void (APIENTRY* glDeleteFramebuffersType) (GLsizei n, const GLuint*);
typedef void (APIENTRY* glDeleteProgramType) (GLuint);
typedef void (APIENTRY* glDeleteRenderbuffersType) (GLsizei n, const GLuint*);
typedef void (APIENTRY* glDeleteShaderType) (GLuint);
typedef void (APIENTRY* glDetachShaderType) (GLuint, GLuint);
typedef void (APIENTRY* glDisableVertexAttribArrayType) (GLuint);
typedef void (APIENTRY* glEnableVertexAttribArrayType) (GLuint);
typedef void (APIENTRY* glFramebufferRenderbufferType) (GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY* glFramebufferTexture2DType) (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY* glGenBuffersType) (GLsizei, GLuint*);
typedef void (APIENTRY* glGenerateMipmapType) (GLenum target);
typedef void (APIENTRY* glGenFramebuffersType) (GLsizei, GLuint*);
typedef void (APIENTRY* glGenRenderbuffersType) (GLsizei, GLuint*);
typedef void (APIENTRY* glGetActiveAttribType) (GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
typedef void (APIENTRY* glGetActiveUniformType) (GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
typedef void (APIENTRY* glGetAttachedShadersType) (GLuint, GLsizei, GLsizei*, GLuint*);
typedef GLint (APIENTRY* glGetAttribLocationType) (GLuint, const char*);
typedef void (APIENTRY* glGetBufferParameterivType) (GLenum, GLenum, GLint*);
typedef void (APIENTRY* glGetFramebufferAttachmentParameterivType) (GLenum, GLenum, GLenum, GLint* params);
typedef void (APIENTRY* glGetProgramInfoLogType) (GLuint, GLsizei, GLsizei*, char*);
typedef void (APIENTRY* glGetProgramivType) (GLuint, GLenum, GLint*);
typedef void (APIENTRY* glGetRenderbufferParameterivType) (GLenum, GLenum, GLint*);
typedef void (APIENTRY* glGetShaderInfoLogType) (GLuint, GLsizei, GLsizei*, char*);
typedef void (APIENTRY* glGetShaderivType) (GLuint, GLenum, GLint*);
typedef void (APIENTRY* glGetShaderSourceType) (GLuint, GLsizei, GLsizei*, char*);
typedef GLint (APIENTRY* glGetUniformLocationType) (GLuint, const char*);
typedef void (APIENTRY* glGetUniformfvType) (GLuint, GLint, GLfloat*);
typedef void (APIENTRY* glGetUniformivType) (GLuint, GLint, GLint*);
typedef void (APIENTRY* glGetVertexAttribfvType) (GLuint, GLenum, GLfloat*);
typedef void (APIENTRY* glGetVertexAttribivType) (GLuint, GLenum, GLint*);
typedef void (APIENTRY* glGetVertexAttribPointervType) (GLuint, GLenum, GLvoid**);
typedef GLboolean (APIENTRY* glIsBufferType) (GLuint);
typedef GLboolean (APIENTRY* glIsFramebufferType) (GLuint);
typedef GLboolean (APIENTRY* glIsProgramType) (GLuint);
typedef GLboolean (APIENTRY* glIsRenderbufferType) (GLuint);
typedef GLboolean (APIENTRY* glIsShaderType) (GLuint);
typedef void (APIENTRY* glLinkProgramType) (GLuint);
typedef void (APIENTRY* glRenderbufferStorageType) (GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY* glSampleCoverageType) (GLclampf, GLboolean);
typedef void (APIENTRY* glShaderSourceType) (GLuint, GLsizei, const char**, const GLint*);
typedef void (APIENTRY* glStencilFuncSeparateType) (GLenum, GLenum, GLint, GLuint);
typedef void (APIENTRY* glStencilMaskSeparateType) (GLenum, GLuint);
typedef void (APIENTRY* glStencilOpSeparateType) (GLenum, GLenum, GLenum, GLenum);
typedef void (APIENTRY* glUniform1fType) (GLint, GLfloat);
typedef void (APIENTRY* glUniform1fvType) (GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* glUniform1iType) (GLint, GLint);
typedef void (APIENTRY* glUniform1ivType) (GLint, GLsizei, const GLint*);
typedef void (APIENTRY* glUniform2fType) (GLint, GLfloat, GLfloat);
typedef void (APIENTRY* glUniform2fvType) (GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* glUniform2iType) (GLint, GLint, GLint);
typedef void (APIENTRY* glUniform2ivType) (GLint, GLsizei, const GLint*);
typedef void (APIENTRY* glUniform3fType) (GLint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* glUniform3fvType) (GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* glUniform3iType) (GLint, GLint, GLint, GLint);
typedef void (APIENTRY* glUniform3ivType) (GLint, GLsizei, const GLint*);
typedef void (APIENTRY* glUniform4fType) (GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* glUniform4fvType) (GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* glUniform4iType) (GLint, GLint, GLint, GLint, GLint);
typedef void (APIENTRY* glUniform4ivType) (GLint, GLsizei, const GLint*);
typedef void (APIENTRY* glUniformMatrix2fvType) (GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (APIENTRY* glUniformMatrix3fvType) (GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (APIENTRY* glUniformMatrix4fvType) (GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (APIENTRY* glUseProgramType) (GLuint);
typedef void (APIENTRY* glValidateProgramType) (GLuint);
typedef void (APIENTRY* glVertexAttrib1fType) (GLuint, const GLfloat);
typedef void (APIENTRY* glVertexAttrib1fvType) (GLuint, const GLfloat*);
typedef void (APIENTRY* glVertexAttrib2fType) (GLuint, const GLfloat, const GLfloat);
typedef void (APIENTRY* glVertexAttrib2fvType) (GLuint, const GLfloat*);
typedef void (APIENTRY* glVertexAttrib3fType) (GLuint, const GLfloat, const GLfloat, const GLfloat);
typedef void (APIENTRY* glVertexAttrib3fvType) (GLuint, const GLfloat*);
typedef void (APIENTRY* glVertexAttrib4fType) (GLuint, const GLfloat, const GLfloat, const GLfloat, const GLfloat);
typedef void (APIENTRY* glVertexAttrib4fvType) (GLuint, const GLfloat*);
typedef void (APIENTRY* glVertexAttribPointerType) (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid*);

class GraphicsContext3DInternal {
public:
    GraphicsContext3DInternal(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow);
    ~GraphicsContext3DInternal();

    bool isContextValid() { return m_contextValid; }
    QGLWidget* getOwnerGLWidget(QWebPageClient* webPageClient);

    glActiveTextureType activeTexture;
    glAttachShaderType attachShader;
    glBindAttribLocationType bindAttribLocation;
    glBindBufferType bindBuffer;
    glBindFramebufferType bindFramebuffer;
    glBindRenderbufferType bindRenderbuffer;
    glBlendColorType blendColor;
    glBlendEquationType blendEquation;
    glBlendEquationSeparateType blendEquationSeparate;
    glBlendFuncSeparateType blendFuncSeparate;
    glBufferDataType bufferData;
    glBufferSubDataType bufferSubData;
    glCheckFramebufferStatusType checkFramebufferStatus;
    glCompileShaderType compileShader;
    glCreateProgramType createProgram;
    glCreateShaderType createShader;
    glDeleteBuffersType deleteBuffers;
    glDeleteFramebuffersType deleteFramebuffers;
    glDeleteProgramType deleteProgram;
    glDeleteRenderbuffersType deleteRenderbuffers;
    glDeleteShaderType deleteShader;
    glDetachShaderType detachShader;
    glDisableVertexAttribArrayType disableVertexAttribArray;
    glEnableVertexAttribArrayType enableVertexAttribArray;
    glFramebufferRenderbufferType framebufferRenderbuffer;
    glFramebufferTexture2DType framebufferTexture2D;
    glGenBuffersType genBuffers;
    glGenerateMipmapType generateMipmap;
    glGenFramebuffersType genFramebuffers;
    glGenRenderbuffersType genRenderbuffers;
    glGetActiveAttribType getActiveAttrib;
    glGetActiveUniformType getActiveUniform;
    glGetAttachedShadersType getAttachedShaders;
    glGetAttribLocationType getAttribLocation;
    glGetBufferParameterivType getBufferParameteriv;
    glGetFramebufferAttachmentParameterivType getFramebufferAttachmentParameteriv;
    glGetProgramInfoLogType getProgramInfoLog;
    glGetProgramivType getProgramiv;
    glGetRenderbufferParameterivType getRenderbufferParameteriv;
    glGetShaderInfoLogType getShaderInfoLog;
    glGetShaderivType getShaderiv;
    glGetShaderSourceType getShaderSource;
    glGetUniformfvType getUniformfv;
    glGetUniformivType getUniformiv;
    glGetUniformLocationType getUniformLocation;
    glGetVertexAttribfvType getVertexAttribfv;
    glGetVertexAttribivType getVertexAttribiv;
    glGetVertexAttribPointervType getVertexAttribPointerv;
    glIsBufferType isBuffer;
    glIsFramebufferType isFramebuffer;
    glIsProgramType isProgram;
    glIsRenderbufferType isRenderbuffer;
    glIsShaderType isShader;
    glLinkProgramType linkProgram;
    glRenderbufferStorageType renderbufferStorage;
    glSampleCoverageType sampleCoverage;
    glShaderSourceType shaderSource;
    glStencilFuncSeparateType stencilFuncSeparate;
    glStencilMaskSeparateType stencilMaskSeparate;
    glStencilOpSeparateType stencilOpSeparate;
    glUniform1fType uniform1f;
    glUniform1fvType uniform1fv;
    glUniform1iType uniform1i;
    glUniform1ivType uniform1iv;
    glUniform2fType uniform2f;
    glUniform2fvType uniform2fv;
    glUniform2iType uniform2i;
    glUniform2ivType uniform2iv;
    glUniform3fType uniform3f;
    glUniform3fvType uniform3fv;
    glUniform3iType uniform3i;
    glUniform3ivType uniform3iv;
    glUniform4fType uniform4f;
    glUniform4fvType uniform4fv;
    glUniform4iType uniform4i;
    glUniform4ivType uniform4iv;
    glUniformMatrix2fvType uniformMatrix2fv;
    glUniformMatrix3fvType uniformMatrix3fv;
    glUniformMatrix4fvType uniformMatrix4fv;
    glUseProgramType useProgram;
    glValidateProgramType validateProgram;
    glVertexAttrib1fType vertexAttrib1f;
    glVertexAttrib1fvType vertexAttrib1fv;
    glVertexAttrib2fType vertexAttrib2f;
    glVertexAttrib2fvType vertexAttrib2fv;
    glVertexAttrib3fType vertexAttrib3f;
    glVertexAttrib3fvType vertexAttrib3fv;
    glVertexAttrib4fType vertexAttrib4f;
    glVertexAttrib4fvType vertexAttrib4fv;
    glVertexAttribPointerType vertexAttribPointer;

    GraphicsContext3D::Attributes m_attrs;
    HostWindow* m_hostWindow;
    QGLWidget* m_glWidget;
    GLuint m_texture;
    GLuint m_mainFbo;
    GLuint m_currentFbo;
    GLuint m_depthBuffer;
    QImage m_pixels;
    ListHashSet<unsigned long> m_syntheticErrors;

private:

    void* getProcAddress(const String& proc);
    bool m_contextValid;
};

#if defined (QT_OPENGL_ES_2) 
#define GET_PROC_ADDRESS(Proc) Proc
#else
#define GET_PROC_ADDRESS(Proc) reinterpret_cast<Proc##Type>(getProcAddress(#Proc));
#endif

bool GraphicsContext3D::isGLES2Compliant() const
{
#if defined (QT_OPENGL_ES_2)
    return true;
#else
    return false;
#endif
}

// Even with underlying GLES2 driver, the below flags should still be set to
// false if extentions exist (and they almost always do).
bool GraphicsContext3D::isGLES2NPOTStrict() const
{
    return false;
}

bool GraphicsContext3D::isErrorGeneratedOnOutOfBoundsAccesses() const
{
    return false;
}

int GraphicsContext3D::getGraphicsResetStatusARB()
{
    return NO_ERROR;
}

 
GraphicsContext3DInternal::GraphicsContext3DInternal(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow)
    : m_attrs(attrs)
    , m_hostWindow(hostWindow)
    , m_glWidget(0)
    , m_texture(0)
    , m_mainFbo(0)
    , m_currentFbo(0)
    , m_depthBuffer(0)
    , m_contextValid(true)
{
    QWebPageClient* webPageClient = hostWindow->platformPageClient();
    QGLWidget* ownerGLWidget  = getOwnerGLWidget(webPageClient);

    if (ownerGLWidget) 
        m_glWidget = new QGLWidget(0, ownerGLWidget);
    else {
        QGLFormat format;
        format.setDepth(true);
        format.setSampleBuffers(true);
        format.setStencil(false);

        m_glWidget = new QGLWidget(format);
    }

    if (!m_glWidget->isValid()) {
        LOG_ERROR("GraphicsContext3D: QGLWidget does not have a valid context");
        m_contextValid = false;
        return;
    }
 
    QGLFormat format = m_glWidget->format();

    m_attrs.alpha = format.alpha();
    m_attrs.depth = format.depth();
    m_attrs.stencil = format.stencil();
    m_attrs.antialias = false;
    m_attrs.premultipliedAlpha = true;

    m_glWidget->makeCurrent();

    activeTexture = GET_PROC_ADDRESS(glActiveTexture);
    attachShader = GET_PROC_ADDRESS(glAttachShader);
    bindAttribLocation = GET_PROC_ADDRESS(glBindAttribLocation);
    bindBuffer = GET_PROC_ADDRESS(glBindBuffer);
    bindFramebuffer = GET_PROC_ADDRESS(glBindFramebuffer);
    bindRenderbuffer = GET_PROC_ADDRESS(glBindRenderbuffer);
    blendColor = GET_PROC_ADDRESS(glBlendColor);
    blendEquation = GET_PROC_ADDRESS(glBlendEquation);
    blendEquationSeparate = GET_PROC_ADDRESS(glBlendEquationSeparate);
    blendFuncSeparate = GET_PROC_ADDRESS(glBlendFuncSeparate);
    bufferData = GET_PROC_ADDRESS(glBufferData);
    bufferSubData = GET_PROC_ADDRESS(glBufferSubData);
    checkFramebufferStatus = GET_PROC_ADDRESS(glCheckFramebufferStatus);
    compileShader = GET_PROC_ADDRESS(glCompileShader);
    createProgram = GET_PROC_ADDRESS(glCreateProgram);
    createShader = GET_PROC_ADDRESS(glCreateShader);
    deleteBuffers = GET_PROC_ADDRESS(glDeleteBuffers);
    deleteFramebuffers = GET_PROC_ADDRESS(glDeleteFramebuffers);
    deleteProgram = GET_PROC_ADDRESS(glDeleteProgram);
    deleteRenderbuffers = GET_PROC_ADDRESS(glDeleteRenderbuffers);
    deleteShader = GET_PROC_ADDRESS(glDeleteShader);
    detachShader = GET_PROC_ADDRESS(glDetachShader);
    disableVertexAttribArray = GET_PROC_ADDRESS(glDisableVertexAttribArray);
    enableVertexAttribArray = GET_PROC_ADDRESS(glEnableVertexAttribArray);
    framebufferRenderbuffer = GET_PROC_ADDRESS(glFramebufferRenderbuffer);
    framebufferTexture2D = GET_PROC_ADDRESS(glFramebufferTexture2D);
    genBuffers = GET_PROC_ADDRESS(glGenBuffers);
    generateMipmap = GET_PROC_ADDRESS(glGenerateMipmap);
    genFramebuffers = GET_PROC_ADDRESS(glGenFramebuffers);
    genRenderbuffers = GET_PROC_ADDRESS(glGenRenderbuffers);
    getActiveAttrib = GET_PROC_ADDRESS(glGetActiveAttrib);
    getActiveUniform = GET_PROC_ADDRESS(glGetActiveUniform);
    getAttachedShaders = GET_PROC_ADDRESS(glGetAttachedShaders);
    getAttribLocation = GET_PROC_ADDRESS(glGetAttribLocation);
    getBufferParameteriv = GET_PROC_ADDRESS(glGetBufferParameteriv);
    getFramebufferAttachmentParameteriv = GET_PROC_ADDRESS(glGetFramebufferAttachmentParameteriv);
    getProgramInfoLog = GET_PROC_ADDRESS(glGetProgramInfoLog);
    getProgramiv = GET_PROC_ADDRESS(glGetProgramiv);
    getRenderbufferParameteriv = GET_PROC_ADDRESS(glGetRenderbufferParameteriv);
    getShaderInfoLog = GET_PROC_ADDRESS(glGetShaderInfoLog);
    getShaderiv = GET_PROC_ADDRESS(glGetShaderiv);
    getShaderSource = GET_PROC_ADDRESS(glGetShaderSource);
    getUniformfv = GET_PROC_ADDRESS(glGetUniformfv);
    getUniformiv = GET_PROC_ADDRESS(glGetUniformiv);
    getUniformLocation = GET_PROC_ADDRESS(glGetUniformLocation);
    getVertexAttribfv = GET_PROC_ADDRESS(glGetVertexAttribfv);
    getVertexAttribiv = GET_PROC_ADDRESS(glGetVertexAttribiv);
    getVertexAttribPointerv = GET_PROC_ADDRESS(glGetVertexAttribPointerv);
    isBuffer = GET_PROC_ADDRESS(glIsBuffer);
    isFramebuffer = GET_PROC_ADDRESS(glIsFramebuffer);
    isProgram = GET_PROC_ADDRESS(glIsProgram);
    isRenderbuffer = GET_PROC_ADDRESS(glIsRenderbuffer);
    isShader = GET_PROC_ADDRESS(glIsShader);
    linkProgram = GET_PROC_ADDRESS(glLinkProgram);
    renderbufferStorage = GET_PROC_ADDRESS(glRenderbufferStorage);
    sampleCoverage = GET_PROC_ADDRESS(glSampleCoverage);
    shaderSource = GET_PROC_ADDRESS(glShaderSource);
    stencilFuncSeparate = GET_PROC_ADDRESS(glStencilFuncSeparate);
    stencilMaskSeparate = GET_PROC_ADDRESS(glStencilMaskSeparate);
    stencilOpSeparate = GET_PROC_ADDRESS(glStencilOpSeparate);
    uniform1f = GET_PROC_ADDRESS(glUniform1f);
    uniform1fv = GET_PROC_ADDRESS(glUniform1fv);
    uniform1i = GET_PROC_ADDRESS(glUniform1i);
    uniform1iv = GET_PROC_ADDRESS(glUniform1iv);
    uniform2f = GET_PROC_ADDRESS(glUniform2f);
    uniform2fv = GET_PROC_ADDRESS(glUniform2fv);
    uniform2i = GET_PROC_ADDRESS(glUniform2i);
    uniform2iv = GET_PROC_ADDRESS(glUniform2iv);
    uniform3f = GET_PROC_ADDRESS(glUniform3f);
    uniform3fv = GET_PROC_ADDRESS(glUniform3fv);
    uniform3i = GET_PROC_ADDRESS(glUniform3i);
    uniform3iv = GET_PROC_ADDRESS(glUniform3iv);
    uniform4f = GET_PROC_ADDRESS(glUniform4f);
    uniform4fv = GET_PROC_ADDRESS(glUniform4fv);
    uniform4i = GET_PROC_ADDRESS(glUniform4i);
    uniform4iv = GET_PROC_ADDRESS(glUniform4iv);
    uniformMatrix2fv = GET_PROC_ADDRESS(glUniformMatrix2fv);
    uniformMatrix3fv = GET_PROC_ADDRESS(glUniformMatrix3fv);
    uniformMatrix4fv = GET_PROC_ADDRESS(glUniformMatrix4fv);
    useProgram = GET_PROC_ADDRESS(glUseProgram);
    validateProgram = GET_PROC_ADDRESS(glValidateProgram);
    vertexAttrib1f = GET_PROC_ADDRESS(glVertexAttrib1f);
    vertexAttrib1fv = GET_PROC_ADDRESS(glVertexAttrib1fv);
    vertexAttrib2f = GET_PROC_ADDRESS(glVertexAttrib2f);
    vertexAttrib2fv = GET_PROC_ADDRESS(glVertexAttrib2fv);
    vertexAttrib3f = GET_PROC_ADDRESS(glVertexAttrib3f);
    vertexAttrib3fv = GET_PROC_ADDRESS(glVertexAttrib3fv);
    vertexAttrib4f = GET_PROC_ADDRESS(glVertexAttrib4f);
    vertexAttrib4fv = GET_PROC_ADDRESS(glVertexAttrib4fv);
    vertexAttribPointer = GET_PROC_ADDRESS(glVertexAttribPointer);

    if (!m_contextValid) {
        LOG_ERROR("GraphicsContext3D: All needed OpenGL extensions are not available");
        m_contextValid = false;
        return;
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GraphicsContext3D::TEXTURE_2D, m_texture);
    glTexParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    glTexParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    glTexParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    glTexParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    glTexImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, 1, 1, 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0);
    glBindTexture(GraphicsContext3D::TEXTURE_2D, 0);

    genFramebuffers(/* count */ 1, &m_mainFbo);
    m_currentFbo = m_mainFbo;

    bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_mainFbo);

    genRenderbuffers(/* count */ 1, &m_depthBuffer);
    bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
#if defined(QT_OPENGL_ES_2)
    renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT16, /* width */ 1, /* height */ 1);
#else
    renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT, /* width */ 1, /* height */ 1);
#endif

    bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
 
    framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_texture, 0);
    framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
    glClearColor(/* red */ 0, /* green */ 0, /* blue */ 0, /* alpha */ 0);

    if (checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("GraphicsContext3D: Wasn't able to create the main framebuffer");
        m_contextValid = false;
    }
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
    delete m_glWidget;
    m_glWidget = 0;
}

QGLWidget* GraphicsContext3DInternal::getOwnerGLWidget(QWebPageClient* webPageClient)
{
    QAbstractScrollArea* scrollArea = qobject_cast<QAbstractScrollArea*>(webPageClient->ownerWidget());

    if (scrollArea) 
        return qobject_cast<QGLWidget*>(scrollArea->viewport());

    return 0;
}

void* GraphicsContext3DInternal::getProcAddress(const String& proc)
{
    String ext[3] = { "", "ARB", "EXT" };

    for (int i = 0; i < 3; i++) {
        String nameWithExt = proc + ext[i];

        void* addr = m_glWidget->context()->getProcAddress(nameWithExt.utf8().data());
        if (addr) 
            return addr;
    }

    LOG_ERROR("GraphicsContext3D: Did not find GL function %s", proc.utf8().data());
    m_contextValid = false;
    return 0;
}

PassOwnPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return 0;
    OwnPtr<GraphicsContext3D> context(new GraphicsContext3D(attrs, hostWindow, false));
    return context->m_internal ? context.release() : 0;
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, bool)
    : m_internal(new GraphicsContext3DInternal(attrs, hostWindow))
{
    if (!m_internal->isContextValid()) 
        m_internal = 0;
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D()
{
    return m_internal->m_glWidget;
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_internal->m_texture;
}

void GraphicsContext3D::makeContextCurrent()
{
    m_internal->m_glWidget->makeCurrent();
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    m_internal->m_glWidget->makeCurrent();
    HTMLCanvasElement* canvas = context->canvas();
    ImageBuffer* imageBuffer = canvas->buffer();
    QPainter* painter = imageBuffer->context()->platformContext();
    paint(painter, QRect(QPoint(0, 0), QSize(m_currentWidth, m_currentHeight)));
}

void GraphicsContext3D::paint(QPainter* painter, const QRect& rect) const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    QWebPageClient* webPageClient = m_internal->m_hostWindow->platformPageClient();
    QGLWidget* ownerGLWidget  = m_internal->getOwnerGLWidget(webPageClient);
    if (ownerGLWidget) {
        ownerGLWidget->drawTexture(rect, m_internal->m_texture);
        return;
    } 
#endif
    m_internal->m_glWidget->makeCurrent();
    m_internal->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_internal->m_mainFbo);
    glReadPixels(/* x */ 0, /* y */ 0, m_currentWidth, m_currentHeight, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, m_internal->m_pixels.bits());
    painter->drawImage(/* x */ 0, /* y */ 0, m_internal->m_pixels.rgbSwapped().transformed(QMatrix().rotate(180)));
    m_internal->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_internal->m_currentFbo); 
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (((width == m_currentWidth) && (height == m_currentHeight)) || (!m_internal))
        return;
    
    m_currentWidth = width;
    m_currentHeight = height;

    m_internal->m_pixels = QImage(m_currentWidth, m_currentHeight, QImage::Format_ARGB32);

    m_internal->m_glWidget->makeCurrent();

    glBindTexture(GraphicsContext3D::TEXTURE_2D, m_internal->m_texture);
    glTexImage2D(GraphicsContext3D::TEXTURE_2D, /* level */ 0, GraphicsContext3D::RGBA, width, height, /* border */ 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, /* data */ 0);
    glBindTexture(GraphicsContext3D::TEXTURE_2D, 0);
   
    m_internal->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_internal->m_mainFbo);
    m_internal->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_internal->m_depthBuffer);
#if defined(QT_OPENGL_ES_2)
    renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT16, width, height);
#else
    renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT, width, height);
#endif
    m_internal->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    
    m_internal->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_internal->m_texture, 0);
    m_internal->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_internal->m_depthBuffer);

    GLenum status = m_internal->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER);
    if (status != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("GraphicsContext3D: Wasn't able to reshape the main framebuffer");
        notImplemented();
    }

    glClear(GraphicsContext3D::COLOR_BUFFER_BIT);
    glFlush();
}

void GraphicsContext3D::activeTexture(unsigned long texture)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->activeTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    m_internal->m_glWidget->makeCurrent();
    m_internal->attachShader((GLuint) program, (GLuint) shader);
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, int maxCount, int* count, unsigned int* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }

    m_internal->m_glWidget->makeCurrent();
    getAttachedShaders((GLuint) program, maxCount, count, shaders);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, unsigned long index, const String& name)
{
    ASSERT(program);
    m_internal->m_glWidget->makeCurrent();
    m_internal->bindAttribLocation((GLuint) program, index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(unsigned long target, Platform3DObject buffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->bindBuffer(target, (GLuint) buffer);
}

void GraphicsContext3D::bindFramebuffer(unsigned long target, Platform3DObject buffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->m_currentFbo = buffer ? (GLuint) buffer : m_internal->m_mainFbo;
    m_internal->bindFramebuffer(target, m_internal->m_currentFbo);
}

void GraphicsContext3D::bindRenderbuffer(unsigned long target, Platform3DObject renderbuffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->bindRenderbuffer(target, (GLuint) renderbuffer);
}

void GraphicsContext3D::bindTexture(unsigned long target, Platform3DObject texture)
{
    m_internal->m_glWidget->makeCurrent();
    glBindTexture(target, (GLuint) texture);
}

void GraphicsContext3D::blendColor(double red, double green, double blue, double alpha)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->blendColor(static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue), static_cast<float>(alpha));
}

void GraphicsContext3D::blendEquation(unsigned long mode)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->blendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->blendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3D::blendFunc(unsigned long sfactor, unsigned long dfactor)
{
    m_internal->m_glWidget->makeCurrent();
    glBlendFunc(sfactor, dfactor);
}       

void GraphicsContext3D::blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(unsigned long target, int size, unsigned long usage)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->bufferData(target, size, /* data */ 0, usage);
}

void GraphicsContext3D::bufferData(unsigned long target, int size, const void* data, unsigned long usage)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->bufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(unsigned long target, long offset, int size, const void* data)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->bufferSubData(target, offset, size, data);
}

unsigned long GraphicsContext3D::checkFramebufferStatus(unsigned long target)
{
    m_internal->m_glWidget->makeCurrent();
    return m_internal->checkFramebufferStatus(target);
}

void GraphicsContext3D::clearColor(double r, double g, double b, double a)
{
    m_internal->m_glWidget->makeCurrent();
    glClearColor(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a));
}

void GraphicsContext3D::clear(unsigned long mask)
{
    m_internal->m_glWidget->makeCurrent();
    glClear(mask);
}

void GraphicsContext3D::clearDepth(double depth)
{
    m_internal->m_glWidget->makeCurrent();
#if defined(QT_OPENGL_ES_2)
    glClearDepthf(depth);
#else
    glClearDepth(depth);
#endif
}

void GraphicsContext3D::clearStencil(long s)
{
    m_internal->m_glWidget->makeCurrent();
    glClearStencil(s);
}

void GraphicsContext3D::colorMask(bool red, bool green, bool blue, bool alpha)
{
    m_internal->m_glWidget->makeCurrent();
    glColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    ASSERT(shader);
    m_internal->m_glWidget->makeCurrent();
    m_internal->compileShader((GLuint) shader);
}

void GraphicsContext3D::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    m_internal->m_glWidget->makeCurrent();
    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    m_internal->m_glWidget->makeCurrent();
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GraphicsContext3D::cullFace(unsigned long mode)
{
    m_internal->m_glWidget->makeCurrent();
    glCullFace(mode);
}

void GraphicsContext3D::depthFunc(unsigned long func)
{
    m_internal->m_glWidget->makeCurrent();
    glDepthFunc(func);
}

void GraphicsContext3D::depthMask(bool flag)
{
    m_internal->m_glWidget->makeCurrent();
    glDepthMask(flag);
}

void GraphicsContext3D::depthRange(double zNear, double zFar)
{
    m_internal->m_glWidget->makeCurrent();
#if defined(QT_OPENGL_ES_2)
    glDepthRangef(zNear, zFar);
#else
    glDepthRange(zNear, zFar);
#endif
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    m_internal->m_glWidget->makeCurrent();
    m_internal->detachShader((GLuint) program, (GLuint) shader);
}

void GraphicsContext3D::disable(unsigned long cap)
{
    m_internal->m_glWidget->makeCurrent();
    glDisable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(unsigned long index)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->disableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(unsigned long mode, long first, long count)
{
    m_internal->m_glWidget->makeCurrent();
    glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    m_internal->m_glWidget->makeCurrent();
    glDrawElements(mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::enable(unsigned long cap)
{
    m_internal->m_glWidget->makeCurrent();
    glEnable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(unsigned long index)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->enableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    m_internal->m_glWidget->makeCurrent();
    glFinish();
}

void GraphicsContext3D::flush()
{
    m_internal->m_glWidget->makeCurrent();
    glFlush();
}

void GraphicsContext3D::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, Platform3DObject buffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->framebufferRenderbuffer(target, attachment, renderbuffertarget, (GLuint) buffer);
}

void GraphicsContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, Platform3DObject texture, long level)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->framebufferTexture2D(target, attachment, textarget, (GLuint) texture, level);
}

void GraphicsContext3D::frontFace(unsigned long mode)
{
    m_internal->m_glWidget->makeCurrent();
    glFrontFace(mode);
}

void GraphicsContext3D::generateMipmap(unsigned long target)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->generateMipmap(target);
}

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, unsigned long index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    m_internal->m_glWidget->makeCurrent();

    GLint maxLength;
    m_internal->getProgramiv(static_cast<GLuint>(program), GraphicsContext3D::ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);

    GLchar* name = (GLchar*) fastMalloc(maxLength);
    GLsizei nameLength;
    GLint size;
    GLenum type;

    m_internal->getActiveAttrib(static_cast<GLuint>(program), index, maxLength, &nameLength, &size, &type, name);

    if (!nameLength) {
        fastFree(name);
        return false;
    }

    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;

    fastFree(name);
    return true;
}
    
bool GraphicsContext3D::getActiveUniform(Platform3DObject program, unsigned long index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    m_internal->m_glWidget->makeCurrent();

    GLint maxLength;
    m_internal->getProgramiv(static_cast<GLuint>(program), GraphicsContext3D::ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);

    GLchar* name = (GLchar*) fastMalloc(maxLength);
    GLsizei nameLength;
    GLint size;
    GLenum type;

    m_internal->getActiveUniform(static_cast<GLuint>(program), index, maxLength, &nameLength, &size, &type, name);

    if (!nameLength) {
        fastFree(name);
        return false;
    }

    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;

    fastFree(name);
    return true;
}

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->getAttribLocation((GLuint) program, name.utf8().data());
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    return m_internal->m_attrs;
}

unsigned long GraphicsContext3D::getError()
{
    if (m_internal->m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_internal->m_syntheticErrors.begin();
        unsigned long err = *iter;
        m_internal->m_syntheticErrors.remove(iter);
        return err;
    }

    m_internal->m_glWidget->makeCurrent();
    return glGetError();
}

String GraphicsContext3D::getString(unsigned long name)
{
    m_internal->m_glWidget->makeCurrent();
    return String((const char*) glGetString(name));
}

void GraphicsContext3D::hint(unsigned long target, unsigned long mode)
{
    m_internal->m_glWidget->makeCurrent();
    glHint(target, mode);
}

bool GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    if (!buffer)
        return false;
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->isBuffer((GLuint) buffer);
}

bool GraphicsContext3D::isEnabled(unsigned long cap)
{
    m_internal->m_glWidget->makeCurrent();
    return glIsEnabled(cap);
}

bool GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    if (!framebuffer)
        return false;
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->isFramebuffer((GLuint) framebuffer);
}

bool GraphicsContext3D::isProgram(Platform3DObject program)
{
    if (!program)
        return false;
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->isProgram((GLuint) program);
}

bool GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    if (!renderbuffer)
        return false;
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->isRenderbuffer((GLuint) renderbuffer);
}

bool GraphicsContext3D::isShader(Platform3DObject shader)
{
    if (!shader)
        return false;
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->isShader((GLuint) shader);
}

bool GraphicsContext3D::isTexture(Platform3DObject texture)
{
    if (!texture)
        return false;
    
    m_internal->m_glWidget->makeCurrent();
    return glIsTexture((GLuint) texture);
}

void GraphicsContext3D::lineWidth(double width)
{
    m_internal->m_glWidget->makeCurrent();
    glLineWidth(static_cast<float>(width));
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    ASSERT(program);
    m_internal->m_glWidget->makeCurrent();
    m_internal->linkProgram((GLuint) program);
}

void GraphicsContext3D::pixelStorei(unsigned long paramName, long param)
{
    m_internal->m_glWidget->makeCurrent();
    glPixelStorei(paramName, param);
}

void GraphicsContext3D::polygonOffset(double factor, double units)
{
    m_internal->m_glWidget->makeCurrent();
    glPolygonOffset(static_cast<float>(factor), static_cast<float>(units));
}

void GraphicsContext3D::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* data)
{
    m_internal->m_glWidget->makeCurrent();
    
    if (type != GraphicsContext3D::UNSIGNED_BYTE || format != GraphicsContext3D::RGBA)
        return;
        
    glReadPixels(x, y, width, height, format, type, (GLvoid*) data);
}

void GraphicsContext3D::releaseShaderCompiler()
{
    m_internal->m_glWidget->makeCurrent();
    notImplemented();
}

void GraphicsContext3D::renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->renderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::sampleCoverage(double value, bool invert)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->sampleCoverage(static_cast<float>(value), invert);
}

void GraphicsContext3D::scissor(long x, long y, unsigned long width, unsigned long height)
{
    m_internal->m_glWidget->makeCurrent();
    glScissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& source)
{
    ASSERT(shader);

    m_internal->m_glWidget->makeCurrent();

    String prefixedSource;

#if defined (QT_OPENGL_ES_2)
    prefixedSource.append("precision mediump float;\n");
#endif

    prefixedSource.append(source);

    CString sourceCS = prefixedSource.utf8();
    const char* data = sourceCS.data();
    int length = prefixedSource.length();
    m_internal->shaderSource((GLuint) shader, /* count */ 1, &data, &length);
}

void GraphicsContext3D::stencilFunc(unsigned long func, long ref, unsigned long mask)
{
    m_internal->m_glWidget->makeCurrent();
    glStencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->stencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(unsigned long mask)
{
    m_internal->m_glWidget->makeCurrent();
    glStencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(unsigned long face, unsigned long mask)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->stencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_internal->m_glWidget->makeCurrent();
    glStencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->stencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContext3D::texParameterf(unsigned target, unsigned paramName, float value)
{
    m_internal->m_glWidget->makeCurrent();
    glTexParameterf(target, paramName, static_cast<float>(value));
}

void GraphicsContext3D::texParameteri(unsigned target, unsigned paramName, int value)
{
    m_internal->m_glWidget->makeCurrent();
    glTexParameteri(target, paramName, static_cast<float>(value));
}

void GraphicsContext3D::uniform1f(long location, float v0)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform1f(location, v0);
}

void GraphicsContext3D::uniform1fv(long location, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform1fv(location, size, array);
}

void GraphicsContext3D::uniform2f(long location, float v0, float v1)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform2f(location, v0, v1);
}

void GraphicsContext3D::uniform2fv(long location, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform2fv(location, size, array);
}

void GraphicsContext3D::uniform3f(long location, float v0, float v1, float v2)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform3f(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3fv(long location, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform3fv(location, size, array);
}

void GraphicsContext3D::uniform4f(long location, float v0, float v1, float v2, float v3)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform4f(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4fv(long location, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform4fv(location, size, array);
}

void GraphicsContext3D::uniform1i(long location, int v0)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform1i(location, v0);
}

void GraphicsContext3D::uniform1iv(long location, int* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform1iv(location, size, array);
}

void GraphicsContext3D::uniform2i(long location, int v0, int v1)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform2i(location, v0, v1);
}

void GraphicsContext3D::uniform2iv(long location, int* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform2iv(location, size, array);
}

void GraphicsContext3D::uniform3i(long location, int v0, int v1, int v2)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform3i(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3iv(long location, int* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform3iv(location, size, array);
}

void GraphicsContext3D::uniform4i(long location, int v0, int v1, int v2, int v3)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform4i(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4iv(long location, int* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniform4iv(location, size, array);
}

void GraphicsContext3D::uniformMatrix2fv(long location, bool transpose, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniformMatrix2fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix3fv(long location, bool transpose, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniformMatrix3fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix4fv(long location, bool transpose, float* array, int size)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->uniformMatrix4fv(location, size, transpose, array);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    ASSERT(program);
    
    m_internal->m_glWidget->makeCurrent();
    m_internal->useProgram((GLuint) program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    ASSERT(program);
    
    m_internal->m_glWidget->makeCurrent();
    m_internal->validateProgram((GLuint) program);
}

void GraphicsContext3D::vertexAttrib1f(unsigned long indx, float v0)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib1f(indx, v0);
}

void GraphicsContext3D::vertexAttrib1fv(unsigned long indx, float* array)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib1fv(indx, array);
}

void GraphicsContext3D::vertexAttrib2f(unsigned long indx, float v0, float v1)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib2f(indx, v0, v1);
}

void GraphicsContext3D::vertexAttrib2fv(unsigned long indx, float* array)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib2fv(indx, array);
}

void GraphicsContext3D::vertexAttrib3f(unsigned long indx, float v0, float v1, float v2)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib3f(indx, v0, v1, v2);
}

void GraphicsContext3D::vertexAttrib3fv(unsigned long indx, float* array)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib3fv(indx, array);
}

void GraphicsContext3D::vertexAttrib4f(unsigned long indx, float v0, float v1, float v2, float v3)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib4f(indx, v0, v1, v2, v3);
}

void GraphicsContext3D::vertexAttrib4fv(unsigned long indx, float* array)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttrib4fv(indx, array);
}

void GraphicsContext3D::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized, unsigned long stride, unsigned long offset)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->vertexAttribPointer(indx, size, type, normalized, stride, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(long x, long y, unsigned long width, unsigned long height)
{
    m_internal->m_glWidget->makeCurrent();
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void GraphicsContext3D::getBooleanv(unsigned long paramName, unsigned char* value)
{
    m_internal->m_glWidget->makeCurrent();
    glGetBooleanv(paramName, value);
}

void GraphicsContext3D::getBufferParameteriv(unsigned long target, unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getBufferParameteriv(target, paramName, value);
}

void GraphicsContext3D::getFloatv(unsigned long paramName, float* value)
{
    m_internal->m_glWidget->makeCurrent();
    glGetFloatv(paramName, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getFramebufferAttachmentParameteriv(target, attachment, paramName, value);
}

void GraphicsContext3D::getIntegerv(unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    glGetIntegerv(paramName, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getProgramiv((GLuint) program, paramName, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    m_internal->m_glWidget->makeCurrent();

    GLint length;
    m_internal->getProgramiv((GLuint) program, GraphicsContext3D::INFO_LOG_LENGTH, &length);

    GLsizei size;

    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    m_internal->getProgramInfoLog((GLuint) program, length, &size, info);

    String result(info);
    fastFree(info);

    return result;
}

void GraphicsContext3D::getRenderbufferParameteriv(unsigned long target, unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getRenderbufferParameteriv(target, paramName, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, unsigned long paramName, int* value)
{
    ASSERT(shader);
    m_internal->m_glWidget->makeCurrent();
    m_internal->getShaderiv((GLuint) shader, paramName, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    m_internal->m_glWidget->makeCurrent();

    GLint length;
    m_internal->getShaderiv((GLuint) shader, GraphicsContext3D::INFO_LOG_LENGTH, &length);

    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    m_internal->getShaderInfoLog((GLuint) shader, length, &size, info);

    String result(info);
    fastFree(info);

    return result;
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    m_internal->m_glWidget->makeCurrent();

    GLint length;
    m_internal->getShaderiv((GLuint) shader, GraphicsContext3D::SHADER_SOURCE_LENGTH, &length);

    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    m_internal->getShaderSource((GLuint) shader, length, &size, info);

    String result(info);
    fastFree(info);

    return result;
}

void GraphicsContext3D::getTexParameterfv(unsigned long target, unsigned long paramName, float* value)
{
    m_internal->m_glWidget->makeCurrent();
    glGetTexParameterfv(target, paramName, value);
}

void GraphicsContext3D::getTexParameteriv(unsigned long target, unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    glGetTexParameteriv(target, paramName, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, long location, float* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getUniformfv((GLuint) program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, long location, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getUniformiv((GLuint) program, location, value);
}

long GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    ASSERT(program);
    
    m_internal->m_glWidget->makeCurrent();
    return m_internal->getUniformLocation((GLuint) program, name.utf8().data());
}

void GraphicsContext3D::getVertexAttribfv(unsigned long index, unsigned long paramName, float* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getVertexAttribfv(index, paramName, value);
}

void GraphicsContext3D::getVertexAttribiv(unsigned long index, unsigned long paramName, int* value)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->getVertexAttribiv(index, paramName, value);
}

long GraphicsContext3D::getVertexAttribOffset(unsigned long index, unsigned long paramName)
{
    m_internal->m_glWidget->makeCurrent();
    
    void* pointer;
    m_internal->getVertexAttribPointerv(index, paramName, &pointer);
    return reinterpret_cast<long>(pointer);
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels)
{
    m_internal->m_glWidget->makeCurrent();
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return 0;
}

   
int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels)
{
    m_internal->m_glWidget->makeCurrent();
    glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
    return 0;
}

unsigned GraphicsContext3D::createBuffer()
{
    m_internal->m_glWidget->makeCurrent();
    GLuint handle;
    m_internal->genBuffers(/* count */ 1, &handle);
    return handle;
}

unsigned GraphicsContext3D::createFramebuffer()
{
    m_internal->m_glWidget->makeCurrent();
    GLuint handle;
    m_internal->genFramebuffers(/* count */ 1, &handle);
    return handle;
}

unsigned GraphicsContext3D::createProgram()
{
    m_internal->m_glWidget->makeCurrent();
    return m_internal->createProgram();
}

unsigned GraphicsContext3D::createRenderbuffer()
{
    m_internal->m_glWidget->makeCurrent();
    GLuint handle;
    m_internal->genRenderbuffers(/* count */ 1, &handle);
    return handle;
}

unsigned GraphicsContext3D::createShader(unsigned long type)
{
    m_internal->m_glWidget->makeCurrent();
    return m_internal->createShader((type == FRAGMENT_SHADER) ? GraphicsContext3D::FRAGMENT_SHADER : GraphicsContext3D::VERTEX_SHADER);
}

unsigned GraphicsContext3D::createTexture()
{
    m_internal->m_glWidget->makeCurrent();
    GLuint handle;
    glGenTextures(1, &handle);
    return handle;
}

void GraphicsContext3D::deleteBuffer(unsigned buffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->deleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(unsigned framebuffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->deleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(unsigned program)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->deleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(unsigned renderbuffer)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->deleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(unsigned shader)
{
    m_internal->m_glWidget->makeCurrent();
    m_internal->deleteShader(shader);
}

void GraphicsContext3D::deleteTexture(unsigned texture)
{
    m_internal->m_glWidget->makeCurrent();
    glDeleteTextures(1, &texture);
}

int GraphicsContext3D::sizeInBytes(int type)
{
    switch (type) {
    case GraphicsContext3D::BYTE:
        return sizeof(GLbyte);
    case GraphicsContext3D::UNSIGNED_BYTE:
        return sizeof(GLubyte);
    case GraphicsContext3D::SHORT:
        return sizeof(GLshort);
    case GraphicsContext3D::UNSIGNED_SHORT:
        return sizeof(GLushort);
    case GraphicsContext3D::INT:
        return sizeof(GLint);
    case GraphicsContext3D::UNSIGNED_INT:
        return sizeof(GLuint);
    case GraphicsContext3D::FLOAT:
        return sizeof(GLfloat);
    default:
        return 0;
    }
}

void GraphicsContext3D::synthesizeGLError(unsigned long error)
{
    m_internal->m_syntheticErrors.add(error);
}

bool GraphicsContext3D::getImageData(Image* image,
                                     unsigned int format,
                                     unsigned int type,
                                     bool premultiplyAlpha,
                                     Vector<uint8_t>& outputVector)
{
    if (!image)
        return false;
    QPixmap* nativePixmap = image->nativeImageForCurrentFrame();
    if (!nativePixmap)
        return false;

    AlphaOp neededAlphaOp = kAlphaDoNothing;
    if (!premultiplyAlpha)
        // FIXME: must fetch the image data before the premultiplication step
        neededAlphaOp = kAlphaDoUnmultiply;
    QImage nativeImage = nativePixmap->toImage().convertToFormat(QImage::Format_ARGB32);
    outputVector.resize(nativeImage.byteCount());
    return packPixels(nativeImage.rgbSwapped().bits(), kSourceFormatRGBA8, image->width(), image->height(), 0,
                      format, type, neededAlphaOp, outputVector.data());
}

}

#endif // ENABLE(3D_CANVAS)
