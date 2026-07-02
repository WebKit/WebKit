/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision 7d0cb181dace7461bc4f26650fec71efeacc18a5.
 */

void glwActiveShaderProgram (GLuint pipeline, GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->activeShaderProgram(pipeline, program);
}

void glwActiveTexture (GLenum texture)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->activeTexture(texture);
}

void glwAttachShader (GLuint program, GLuint shader)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->attachShader(program, shader);
}

void glwBeginConditionalRender (GLuint id, GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->beginConditionalRender(id, mode);
}

void glwBeginQuery (GLenum target, GLuint id)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->beginQuery(target, id);
}

void glwBeginQueryIndexed (GLenum target, GLuint index, GLuint id)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->beginQueryIndexed(target, index, id);
}

void glwBeginTransformFeedback (GLenum primitiveMode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->beginTransformFeedback(primitiveMode);
}

void glwBindAttribLocation (GLuint program, GLuint index, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindAttribLocation(program, index, name);
}

void glwBindBuffer (GLenum target, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindBuffer(target, buffer);
}

void glwBindBufferBase (GLenum target, GLuint index, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindBufferBase(target, index, buffer);
}

void glwBindBufferRange (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindBufferRange(target, index, buffer, offset, size);
}

void glwBindBuffersBase (GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindBuffersBase(target, first, count, buffers);
}

void glwBindBuffersRange (GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindBuffersRange(target, first, count, buffers, offsets, sizes);
}

void glwBindFragDataLocation (GLuint program, GLuint color, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindFragDataLocation(program, color, name);
}

void glwBindFragDataLocationIndexed (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindFragDataLocationIndexed(program, colorNumber, index, name);
}

void glwBindFramebuffer (GLenum target, GLuint framebuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindFramebuffer(target, framebuffer);
}

void glwBindImageTexture (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindImageTexture(unit, texture, level, layered, layer, access, format);
}

void glwBindImageTextures (GLuint first, GLsizei count, const GLuint *textures)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindImageTextures(first, count, textures);
}

void glwBindMultiTextureEXT (GLenum texunit, GLenum target, GLuint texture)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindMultiTextureEXT(texunit, target, texture);
}

void glwBindProgramPipeline (GLuint pipeline)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindProgramPipeline(pipeline);
}

void glwBindRenderbuffer (GLenum target, GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindRenderbuffer(target, renderbuffer);
}

void glwBindSampler (GLuint unit, GLuint sampler)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindSampler(unit, sampler);
}

void glwBindSamplers (GLuint first, GLsizei count, const GLuint *samplers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindSamplers(first, count, samplers);
}

void glwBindTexture (GLenum target, GLuint texture)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindTexture(target, texture);
}

void glwBindTextureUnit (GLuint unit, GLuint texture)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindTextureUnit(unit, texture);
}

void glwBindTextures (GLuint first, GLsizei count, const GLuint *textures)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindTextures(first, count, textures);
}

void glwBindTransformFeedback (GLenum target, GLuint id)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindTransformFeedback(target, id);
}

void glwBindVertexArray (GLuint array)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindVertexArray(array);
}

void glwBindVertexBuffer (GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindVertexBuffer(bindingindex, buffer, offset, stride);
}

void glwBindVertexBuffers (GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bindVertexBuffers(first, count, buffers, offsets, strides);
}

void glwBlendBarrier (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendBarrier();
}

void glwBlendColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendColor(red, green, blue, alpha);
}

void glwBlendEquation (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendEquation(mode);
}

void glwBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendEquationSeparate(modeRGB, modeAlpha);
}

void glwBlendEquationSeparatei (GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendEquationSeparatei(buf, modeRGB, modeAlpha);
}

void glwBlendEquationi (GLuint buf, GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendEquationi(buf, mode);
}

void glwBlendFunc (GLenum sfactor, GLenum dfactor)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendFunc(sfactor, dfactor);
}

void glwBlendFuncSeparate (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

void glwBlendFuncSeparatei (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendFuncSeparatei(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glwBlendFunci (GLuint buf, GLenum src, GLenum dst)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blendFunci(buf, src, dst);
}

void glwBlitFramebuffer (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void glwBlitNamedFramebuffer (GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->blitNamedFramebuffer(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void glwBufferData (GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bufferData(target, size, data, usage);
}

void glwBufferPageCommitmentARB (GLenum target, GLintptr offset, GLsizeiptr size, GLboolean commit)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bufferPageCommitmentARB(target, offset, size, commit);
}

void glwBufferStorage (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bufferStorage(target, size, data, flags);
}

void glwBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->bufferSubData(target, offset, size, data);
}

GLenum glwCheckFramebufferStatus (GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLenum)0;
    return gl->checkFramebufferStatus(target);
}

GLenum glwCheckNamedFramebufferStatus (GLuint framebuffer, GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLenum)0;
    return gl->checkNamedFramebufferStatus(framebuffer, target);
}

GLenum glwCheckNamedFramebufferStatusEXT (GLuint framebuffer, GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLenum)0;
    return gl->checkNamedFramebufferStatusEXT(framebuffer, target);
}

void glwClampColor (GLenum target, GLenum clamp)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clampColor(target, clamp);
}

void glwClear (GLbitfield mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clear(mask);
}

void glwClearBufferData (GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearBufferData(target, internalformat, format, type, data);
}

void glwClearBufferSubData (GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearBufferSubData(target, internalformat, offset, size, format, type, data);
}

void glwClearBufferfi (GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearBufferfi(buffer, drawbuffer, depth, stencil);
}

void glwClearBufferfv (GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearBufferfv(buffer, drawbuffer, value);
}

void glwClearBufferiv (GLenum buffer, GLint drawbuffer, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearBufferiv(buffer, drawbuffer, value);
}

void glwClearBufferuiv (GLenum buffer, GLint drawbuffer, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearBufferuiv(buffer, drawbuffer, value);
}

void glwClearColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearColor(red, green, blue, alpha);
}

void glwClearDepth (GLdouble depth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearDepth(depth);
}

void glwClearDepthf (GLfloat d)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearDepthf(d);
}

void glwClearNamedBufferData (GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedBufferData(buffer, internalformat, format, type, data);
}

void glwClearNamedBufferDataEXT (GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedBufferDataEXT(buffer, internalformat, format, type, data);
}

void glwClearNamedBufferSubData (GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedBufferSubData(buffer, internalformat, offset, size, format, type, data);
}

void glwClearNamedBufferSubDataEXT (GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);
}

void glwClearNamedFramebufferfi (GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedFramebufferfi(framebuffer, buffer, drawbuffer, depth, stencil);
}

void glwClearNamedFramebufferfv (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);
}

void glwClearNamedFramebufferiv (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value);
}

void glwClearNamedFramebufferuiv (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value);
}

void glwClearStencil (GLint s)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearStencil(s);
}

void glwClearTexImage (GLuint texture, GLint level, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearTexImage(texture, level, format, type, data);
}

void glwClearTexSubImage (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
}

void glwClientAttribDefaultEXT (GLbitfield mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clientAttribDefaultEXT(mask);
}

GLenum glwClientWaitSync (GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLenum)0;
    return gl->clientWaitSync(sync, flags, timeout);
}

void glwClipControl (GLenum origin, GLenum depth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->clipControl(origin, depth);
}

void glwColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->colorMask(red, green, blue, alpha);
}

void glwColorMaski (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->colorMaski(index, r, g, b, a);
}

void glwCompileShader (GLuint shader)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compileShader(shader);
}

void glwCompressedMultiTexImage1DEXT (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedMultiTexImage1DEXT(texunit, target, level, internalformat, width, border, imageSize, bits);
}

void glwCompressedMultiTexImage2DEXT (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedMultiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, imageSize, bits);
}

void glwCompressedMultiTexImage3DEXT (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedMultiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void glwCompressedMultiTexSubImage1DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, imageSize, bits);
}

void glwCompressedMultiTexSubImage2DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void glwCompressedMultiTexSubImage3DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void glwCompressedTexImage1D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexImage1D(target, level, internalformat, width, border, imageSize, data);
}

void glwCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void glwCompressedTexImage3D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void glwCompressedTexImage3DOES (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexImage3DOES(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void glwCompressedTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
}

void glwCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void glwCompressedTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void glwCompressedTexSubImage3DOES (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void glwCompressedTextureImage1DEXT (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureImage1DEXT(texture, target, level, internalformat, width, border, imageSize, bits);
}

void glwCompressedTextureImage2DEXT (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureImage2DEXT(texture, target, level, internalformat, width, height, border, imageSize, bits);
}

void glwCompressedTextureImage3DEXT (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void glwCompressedTextureSubImage1D (GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureSubImage1D(texture, level, xoffset, width, format, imageSize, data);
}

void glwCompressedTextureSubImage1DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureSubImage1DEXT(texture, target, level, xoffset, width, format, imageSize, bits);
}

void glwCompressedTextureSubImage2D (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void glwCompressedTextureSubImage2DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void glwCompressedTextureSubImage3D (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void glwCompressedTextureSubImage3DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->compressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void glwCopyBufferSubData (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void glwCopyImageSubData (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void glwCopyMultiTexImage1DEXT (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyMultiTexImage1DEXT(texunit, target, level, internalformat, x, y, width, border);
}

void glwCopyMultiTexImage2DEXT (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyMultiTexImage2DEXT(texunit, target, level, internalformat, x, y, width, height, border);
}

void glwCopyMultiTexSubImage1DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyMultiTexSubImage1DEXT(texunit, target, level, xoffset, x, y, width);
}

void glwCopyMultiTexSubImage2DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, x, y, width, height);
}

void glwCopyMultiTexSubImage3DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glwCopyNamedBufferSubData (GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyNamedBufferSubData(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void glwCopyTexImage1D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTexImage1D(target, level, internalformat, x, y, width, border);
}

void glwCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void glwCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTexSubImage1D(target, level, xoffset, x, y, width);
}

void glwCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void glwCopyTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glwCopyTexSubImage3DOES (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glwCopyTextureImage1DEXT (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureImage1DEXT(texture, target, level, internalformat, x, y, width, border);
}

void glwCopyTextureImage2DEXT (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureImage2DEXT(texture, target, level, internalformat, x, y, width, height, border);
}

void glwCopyTextureSubImage1D (GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureSubImage1D(texture, level, xoffset, x, y, width);
}

void glwCopyTextureSubImage1DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureSubImage1DEXT(texture, target, level, xoffset, x, y, width);
}

void glwCopyTextureSubImage2D (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureSubImage2D(texture, level, xoffset, yoffset, x, y, width, height);
}

void glwCopyTextureSubImage2DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, x, y, width, height);
}

void glwCopyTextureSubImage3D (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glwCopyTextureSubImage3DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->copyTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glwCreateBuffers (GLsizei n, GLuint *buffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createBuffers(n, buffers);
}

void glwCreateFramebuffers (GLsizei n, GLuint *framebuffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createFramebuffers(n, framebuffers);
}

GLuint glwCreateProgram (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->createProgram();
}

void glwCreateProgramPipelines (GLsizei n, GLuint *pipelines)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createProgramPipelines(n, pipelines);
}

void glwCreateQueries (GLenum target, GLsizei n, GLuint *ids)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createQueries(target, n, ids);
}

void glwCreateRenderbuffers (GLsizei n, GLuint *renderbuffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createRenderbuffers(n, renderbuffers);
}

void glwCreateSamplers (GLsizei n, GLuint *samplers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createSamplers(n, samplers);
}

GLuint glwCreateShader (GLenum type)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->createShader(type);
}

GLuint glwCreateShaderProgramv (GLenum type, GLsizei count, const GLchar *const*strings)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->createShaderProgramv(type, count, strings);
}

void glwCreateTextures (GLenum target, GLsizei n, GLuint *textures)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createTextures(target, n, textures);
}

void glwCreateTransformFeedbacks (GLsizei n, GLuint *ids)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createTransformFeedbacks(n, ids);
}

void glwCreateVertexArrays (GLsizei n, GLuint *arrays)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->createVertexArrays(n, arrays);
}

void glwCullFace (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->cullFace(mode);
}

void glwDebugMessageCallback (GLDEBUGPROC callback, const void *userParam)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->debugMessageCallback(callback, userParam);
}

void glwDebugMessageControl (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->debugMessageControl(source, type, severity, count, ids, enabled);
}

void glwDebugMessageInsert (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->debugMessageInsert(source, type, id, severity, length, buf);
}

void glwDeleteBuffers (GLsizei n, const GLuint *buffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteBuffers(n, buffers);
}

void glwDeleteFramebuffers (GLsizei n, const GLuint *framebuffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteFramebuffers(n, framebuffers);
}

void glwDeleteProgram (GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteProgram(program);
}

void glwDeleteProgramPipelines (GLsizei n, const GLuint *pipelines)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteProgramPipelines(n, pipelines);
}

void glwDeleteQueries (GLsizei n, const GLuint *ids)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteQueries(n, ids);
}

void glwDeleteRenderbuffers (GLsizei n, const GLuint *renderbuffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteRenderbuffers(n, renderbuffers);
}

void glwDeleteSamplers (GLsizei count, const GLuint *samplers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteSamplers(count, samplers);
}

void glwDeleteShader (GLuint shader)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteShader(shader);
}

void glwDeleteSync (GLsync sync)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteSync(sync);
}

void glwDeleteTextures (GLsizei n, const GLuint *textures)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteTextures(n, textures);
}

void glwDeleteTransformFeedbacks (GLsizei n, const GLuint *ids)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteTransformFeedbacks(n, ids);
}

void glwDeleteVertexArrays (GLsizei n, const GLuint *arrays)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->deleteVertexArrays(n, arrays);
}

void glwDepthBoundsEXT (GLclampd zmin, GLclampd zmax)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthBoundsEXT(zmin, zmax);
}

void glwDepthFunc (GLenum func)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthFunc(func);
}

void glwDepthMask (GLboolean flag)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthMask(flag);
}

void glwDepthRange (GLdouble n, GLdouble f)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthRange(n, f);
}

void glwDepthRangeArrayfvOES (GLuint first, GLsizei count, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthRangeArrayfvOES(first, count, v);
}

void glwDepthRangeArrayv (GLuint first, GLsizei count, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthRangeArrayv(first, count, v);
}

void glwDepthRangeIndexed (GLuint index, GLdouble n, GLdouble f)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthRangeIndexed(index, n, f);
}

void glwDepthRangeIndexedfOES (GLuint index, GLfloat n, GLfloat f)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthRangeIndexedfOES(index, n, f);
}

void glwDepthRangef (GLfloat n, GLfloat f)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->depthRangef(n, f);
}

void glwDetachShader (GLuint program, GLuint shader)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->detachShader(program, shader);
}

void glwDisable (GLenum cap)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disable(cap);
}

void glwDisableClientStateIndexedEXT (GLenum array, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disableClientStateIndexedEXT(array, index);
}

void glwDisableClientStateiEXT (GLenum array, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disableClientStateiEXT(array, index);
}

void glwDisableVertexArrayAttrib (GLuint vaobj, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disableVertexArrayAttrib(vaobj, index);
}

void glwDisableVertexArrayAttribEXT (GLuint vaobj, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disableVertexArrayAttribEXT(vaobj, index);
}

void glwDisableVertexArrayEXT (GLuint vaobj, GLenum array)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disableVertexArrayEXT(vaobj, array);
}

void glwDisableVertexAttribArray (GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disableVertexAttribArray(index);
}

void glwDisablei (GLenum target, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->disablei(target, index);
}

void glwDispatchCompute (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->dispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

void glwDispatchComputeIndirect (GLintptr indirect)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->dispatchComputeIndirect(indirect);
}

void glwDrawArrays (GLenum mode, GLint first, GLsizei count)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawArrays(mode, first, count);
}

void glwDrawArraysIndirect (GLenum mode, const void *indirect)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawArraysIndirect(mode, indirect);
}

void glwDrawArraysInstanced (GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawArraysInstanced(mode, first, count, instancecount);
}

void glwDrawArraysInstancedBaseInstance (GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);
}

void glwDrawBuffer (GLenum buf)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawBuffer(buf);
}

void glwDrawBuffers (GLsizei n, const GLenum *bufs)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawBuffers(n, bufs);
}

void glwDrawElements (GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElements(mode, count, type, indices);
}

void glwDrawElementsBaseVertex (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElementsBaseVertex(mode, count, type, indices, basevertex);
}

void glwDrawElementsIndirect (GLenum mode, GLenum type, const void *indirect)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElementsIndirect(mode, type, indirect);
}

void glwDrawElementsInstanced (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElementsInstanced(mode, count, type, indices, instancecount);
}

void glwDrawElementsInstancedBaseInstance (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);
}

void glwDrawElementsInstancedBaseVertex (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
}

void glwDrawElementsInstancedBaseVertexBaseInstance (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);
}

void glwDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawRangeElements(mode, start, end, count, type, indices);
}

void glwDrawRangeElementsBaseVertex (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);
}

void glwDrawTransformFeedback (GLenum mode, GLuint id)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawTransformFeedback(mode, id);
}

void glwDrawTransformFeedbackInstanced (GLenum mode, GLuint id, GLsizei instancecount)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawTransformFeedbackInstanced(mode, id, instancecount);
}

void glwDrawTransformFeedbackStream (GLenum mode, GLuint id, GLuint stream)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawTransformFeedbackStream(mode, id, stream);
}

void glwDrawTransformFeedbackStreamInstanced (GLenum mode, GLuint id, GLuint stream, GLsizei instancecount)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->drawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);
}

void glwEGLImageTargetRenderbufferStorageOES (GLenum target, GLeglImageOES image)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->eglImageTargetRenderbufferStorageOES(target, image);
}

void glwEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->eglImageTargetTexture2DOES(target, image);
}

void glwEnable (GLenum cap)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enable(cap);
}

void glwEnableClientStateIndexedEXT (GLenum array, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enableClientStateIndexedEXT(array, index);
}

void glwEnableClientStateiEXT (GLenum array, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enableClientStateiEXT(array, index);
}

void glwEnableVertexArrayAttrib (GLuint vaobj, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enableVertexArrayAttrib(vaobj, index);
}

void glwEnableVertexArrayAttribEXT (GLuint vaobj, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enableVertexArrayAttribEXT(vaobj, index);
}

void glwEnableVertexArrayEXT (GLuint vaobj, GLenum array)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enableVertexArrayEXT(vaobj, array);
}

void glwEnableVertexAttribArray (GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enableVertexAttribArray(index);
}

void glwEnablei (GLenum target, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->enablei(target, index);
}

void glwEndConditionalRender (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->endConditionalRender();
}

void glwEndQuery (GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->endQuery(target);
}

void glwEndQueryIndexed (GLenum target, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->endQueryIndexed(target, index);
}

void glwEndTransformFeedback (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->endTransformFeedback();
}

GLsync glwFenceSync (GLenum condition, GLbitfield flags)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLsync)0;
    return gl->fenceSync(condition, flags);
}

void glwFinish (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->finish();
}

void glwFlush (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->flush();
}

void glwFlushMappedBufferRange (GLenum target, GLintptr offset, GLsizeiptr length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->flushMappedBufferRange(target, offset, length);
}

void glwFlushMappedNamedBufferRange (GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->flushMappedNamedBufferRange(buffer, offset, length);
}

void glwFlushMappedNamedBufferRangeEXT (GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->flushMappedNamedBufferRangeEXT(buffer, offset, length);
}

void glwFramebufferDrawBufferEXT (GLuint framebuffer, GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferDrawBufferEXT(framebuffer, mode);
}

void glwFramebufferDrawBuffersEXT (GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferDrawBuffersEXT(framebuffer, n, bufs);
}

void glwFramebufferParameteri (GLenum target, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferParameteri(target, pname, param);
}

void glwFramebufferReadBufferEXT (GLuint framebuffer, GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferReadBufferEXT(framebuffer, mode);
}

void glwFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void glwFramebufferShadingRateEXT (GLenum target, GLenum attachment, GLuint texture, GLint baseLayer, GLsizei numLayers, GLsizei texelWidth, GLsizei texelHeight)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferShadingRateEXT(target, attachment, texture, baseLayer, numLayers, texelWidth, texelHeight);
}

void glwFramebufferTexture (GLenum target, GLenum attachment, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTexture(target, attachment, texture, level);
}

void glwFramebufferTexture1D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTexture1D(target, attachment, textarget, texture, level);
}

void glwFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTexture2D(target, attachment, textarget, texture, level);
}

void glwFramebufferTexture2DMultisampleEXT (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTexture2DMultisampleEXT(target, attachment, textarget, texture, level, samples);
}

void glwFramebufferTexture3D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
}

void glwFramebufferTexture3DOES (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTexture3DOES(target, attachment, textarget, texture, level, zoffset);
}

void glwFramebufferTextureLayer (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTextureLayer(target, attachment, texture, level, layer);
}

void glwFramebufferTextureMultisampleMultiviewOVR (GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTextureMultisampleMultiviewOVR(target, attachment, texture, level, samples, baseViewIndex, numViews);
}

void glwFramebufferTextureMultiviewOVR (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->framebufferTextureMultiviewOVR(target, attachment, texture, level, baseViewIndex, numViews);
}

void glwFrontFace (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->frontFace(mode);
}

void glwGenBuffers (GLsizei n, GLuint *buffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genBuffers(n, buffers);
}

void glwGenFramebuffers (GLsizei n, GLuint *framebuffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genFramebuffers(n, framebuffers);
}

void glwGenProgramPipelines (GLsizei n, GLuint *pipelines)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genProgramPipelines(n, pipelines);
}

void glwGenQueries (GLsizei n, GLuint *ids)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genQueries(n, ids);
}

void glwGenRenderbuffers (GLsizei n, GLuint *renderbuffers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genRenderbuffers(n, renderbuffers);
}

void glwGenSamplers (GLsizei count, GLuint *samplers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genSamplers(count, samplers);
}

void glwGenTextures (GLsizei n, GLuint *textures)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genTextures(n, textures);
}

void glwGenTransformFeedbacks (GLsizei n, GLuint *ids)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genTransformFeedbacks(n, ids);
}

void glwGenVertexArrays (GLsizei n, GLuint *arrays)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->genVertexArrays(n, arrays);
}

void glwGenerateMipmap (GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->generateMipmap(target);
}

void glwGenerateMultiTexMipmapEXT (GLenum texunit, GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->generateMultiTexMipmapEXT(texunit, target);
}

void glwGenerateTextureMipmap (GLuint texture)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->generateTextureMipmap(texture);
}

void glwGenerateTextureMipmapEXT (GLuint texture, GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->generateTextureMipmapEXT(texture, target);
}

void glwGetActiveAtomicCounterBufferiv (GLuint program, GLuint bufferIndex, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveAtomicCounterBufferiv(program, bufferIndex, pname, params);
}

void glwGetActiveAttrib (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveAttrib(program, index, bufSize, length, size, type, name);
}

void glwGetActiveSubroutineName (GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveSubroutineName(program, shadertype, index, bufSize, length, name);
}

void glwGetActiveSubroutineUniformName (GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveSubroutineUniformName(program, shadertype, index, bufSize, length, name);
}

void glwGetActiveSubroutineUniformiv (GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveSubroutineUniformiv(program, shadertype, index, pname, values);
}

void glwGetActiveUniform (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveUniform(program, index, bufSize, length, size, type, name);
}

void glwGetActiveUniformBlockName (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void glwGetActiveUniformBlockiv (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void glwGetActiveUniformName (GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformName)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
}

void glwGetActiveUniformsiv (GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

void glwGetAttachedShaders (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getAttachedShaders(program, maxCount, count, shaders);
}

GLint glwGetAttribLocation (GLuint program, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getAttribLocation(program, name);
}

void glwGetBooleani_v (GLenum target, GLuint index, GLboolean *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getBooleani_v(target, index, data);
}

void glwGetBooleanv (GLenum pname, GLboolean *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getBooleanv(pname, data);
}

void glwGetBufferParameteri64v (GLenum target, GLenum pname, GLint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getBufferParameteri64v(target, pname, params);
}

void glwGetBufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getBufferParameteriv(target, pname, params);
}

void glwGetBufferPointerv (GLenum target, GLenum pname, void **params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getBufferPointerv(target, pname, params);
}

void glwGetBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getBufferSubData(target, offset, size, data);
}

void glwGetCompressedMultiTexImageEXT (GLenum texunit, GLenum target, GLint lod, void *img)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getCompressedMultiTexImageEXT(texunit, target, lod, img);
}

void glwGetCompressedTexImage (GLenum target, GLint level, void *img)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getCompressedTexImage(target, level, img);
}

void glwGetCompressedTextureImage (GLuint texture, GLint level, GLsizei bufSize, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getCompressedTextureImage(texture, level, bufSize, pixels);
}

void glwGetCompressedTextureImageEXT (GLuint texture, GLenum target, GLint lod, void *img)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getCompressedTextureImageEXT(texture, target, lod, img);
}

void glwGetCompressedTextureSubImage (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getCompressedTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, bufSize, pixels);
}

GLuint glwGetDebugMessageLog (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->getDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
}

void glwGetDoublei_v (GLenum target, GLuint index, GLdouble *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getDoublei_v(target, index, data);
}

void glwGetDoublev (GLenum pname, GLdouble *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getDoublev(pname, data);
}

GLenum glwGetError (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return GL_INVALID_OPERATION;
    return gl->getError();
}

void glwGetFloati_v (GLenum target, GLuint index, GLfloat *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getFloati_v(target, index, data);
}

void glwGetFloatv (GLenum pname, GLfloat *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getFloatv(pname, data);
}

GLint glwGetFragDataIndex (GLuint program, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getFragDataIndex(program, name);
}

GLint glwGetFragDataLocation (GLuint program, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getFragDataLocation(program, name);
}

void glwGetFragmentShadingRatesEXT (GLsizei samples, GLsizei maxCount, GLsizei *count, GLenum *shadingRates)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getFragmentShadingRatesEXT(samples, maxCount, count, shadingRates);
}

void glwGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void glwGetFramebufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getFramebufferParameteriv(target, pname, params);
}

void glwGetFramebufferParameterivEXT (GLuint framebuffer, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getFramebufferParameterivEXT(framebuffer, pname, params);
}

GLenum glwGetGraphicsResetStatus (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLenum)0;
    return gl->getGraphicsResetStatus();
}

void glwGetInteger64i_v (GLenum target, GLuint index, GLint64 *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getInteger64i_v(target, index, data);
}

void glwGetInteger64v (GLenum pname, GLint64 *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getInteger64v(pname, data);
}

void glwGetIntegeri_v (GLenum target, GLuint index, GLint *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getIntegeri_v(target, index, data);
}

void glwGetIntegerv (GLenum pname, GLint *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getIntegerv(pname, data);
}

void glwGetInternalformatSampleivNV (GLenum target, GLenum internalformat, GLsizei samples, GLenum pname, GLsizei count, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getInternalformatSampleivNV(target, internalformat, samples, pname, count, params);
}

void glwGetInternalformati64v (GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getInternalformati64v(target, internalformat, pname, count, params);
}

void glwGetInternalformativ (GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getInternalformativ(target, internalformat, pname, count, params);
}

void glwGetMultiTexEnvfvEXT (GLenum texunit, GLenum target, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexEnvfvEXT(texunit, target, pname, params);
}

void glwGetMultiTexEnvivEXT (GLenum texunit, GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexEnvivEXT(texunit, target, pname, params);
}

void glwGetMultiTexGendvEXT (GLenum texunit, GLenum coord, GLenum pname, GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexGendvEXT(texunit, coord, pname, params);
}

void glwGetMultiTexGenfvEXT (GLenum texunit, GLenum coord, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexGenfvEXT(texunit, coord, pname, params);
}

void glwGetMultiTexGenivEXT (GLenum texunit, GLenum coord, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexGenivEXT(texunit, coord, pname, params);
}

void glwGetMultiTexImageEXT (GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexImageEXT(texunit, target, level, format, type, pixels);
}

void glwGetMultiTexLevelParameterfvEXT (GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexLevelParameterfvEXT(texunit, target, level, pname, params);
}

void glwGetMultiTexLevelParameterivEXT (GLenum texunit, GLenum target, GLint level, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexLevelParameterivEXT(texunit, target, level, pname, params);
}

void glwGetMultiTexParameterIivEXT (GLenum texunit, GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexParameterIivEXT(texunit, target, pname, params);
}

void glwGetMultiTexParameterIuivEXT (GLenum texunit, GLenum target, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexParameterIuivEXT(texunit, target, pname, params);
}

void glwGetMultiTexParameterfvEXT (GLenum texunit, GLenum target, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexParameterfvEXT(texunit, target, pname, params);
}

void glwGetMultiTexParameterivEXT (GLenum texunit, GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultiTexParameterivEXT(texunit, target, pname, params);
}

void glwGetMultisamplefv (GLenum pname, GLuint index, GLfloat *val)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getMultisamplefv(pname, index, val);
}

void glwGetNamedBufferParameteri64v (GLuint buffer, GLenum pname, GLint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferParameteri64v(buffer, pname, params);
}

void glwGetNamedBufferParameteriv (GLuint buffer, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferParameteriv(buffer, pname, params);
}

void glwGetNamedBufferParameterivEXT (GLuint buffer, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferParameterivEXT(buffer, pname, params);
}

void glwGetNamedBufferPointerv (GLuint buffer, GLenum pname, void **params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferPointerv(buffer, pname, params);
}

void glwGetNamedBufferPointervEXT (GLuint buffer, GLenum pname, void **params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferPointervEXT(buffer, pname, params);
}

void glwGetNamedBufferSubData (GLuint buffer, GLintptr offset, GLsizeiptr size, void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferSubData(buffer, offset, size, data);
}

void glwGetNamedBufferSubDataEXT (GLuint buffer, GLintptr offset, GLsizeiptr size, void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedBufferSubDataEXT(buffer, offset, size, data);
}

void glwGetNamedFramebufferAttachmentParameteriv (GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedFramebufferAttachmentParameteriv(framebuffer, attachment, pname, params);
}

void glwGetNamedFramebufferAttachmentParameterivEXT (GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment, pname, params);
}

void glwGetNamedFramebufferParameteriv (GLuint framebuffer, GLenum pname, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedFramebufferParameteriv(framebuffer, pname, param);
}

void glwGetNamedFramebufferParameterivEXT (GLuint framebuffer, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedFramebufferParameterivEXT(framebuffer, pname, params);
}

void glwGetNamedProgramLocalParameterIivEXT (GLuint program, GLenum target, GLuint index, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedProgramLocalParameterIivEXT(program, target, index, params);
}

void glwGetNamedProgramLocalParameterIuivEXT (GLuint program, GLenum target, GLuint index, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedProgramLocalParameterIuivEXT(program, target, index, params);
}

void glwGetNamedProgramLocalParameterdvEXT (GLuint program, GLenum target, GLuint index, GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedProgramLocalParameterdvEXT(program, target, index, params);
}

void glwGetNamedProgramLocalParameterfvEXT (GLuint program, GLenum target, GLuint index, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedProgramLocalParameterfvEXT(program, target, index, params);
}

void glwGetNamedProgramStringEXT (GLuint program, GLenum target, GLenum pname, void *string)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedProgramStringEXT(program, target, pname, string);
}

void glwGetNamedProgramivEXT (GLuint program, GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedProgramivEXT(program, target, pname, params);
}

void glwGetNamedRenderbufferParameteriv (GLuint renderbuffer, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedRenderbufferParameteriv(renderbuffer, pname, params);
}

void glwGetNamedRenderbufferParameterivEXT (GLuint renderbuffer, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getNamedRenderbufferParameterivEXT(renderbuffer, pname, params);
}

void glwGetObjectLabel (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getObjectLabel(identifier, name, bufSize, length, label);
}

void glwGetObjectPtrLabel (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getObjectPtrLabel(ptr, bufSize, length, label);
}

void glwGetPointerIndexedvEXT (GLenum target, GLuint index, void **data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getPointerIndexedvEXT(target, index, data);
}

void glwGetPointeri_vEXT (GLenum pname, GLuint index, void **params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getPointeri_vEXT(pname, index, params);
}

void glwGetPointerv (GLenum pname, void **params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getPointerv(pname, params);
}

void glwGetProgramBinary (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void glwGetProgramInfoLog (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramInfoLog(program, bufSize, length, infoLog);
}

void glwGetProgramInterfaceiv (GLuint program, GLenum programInterface, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramInterfaceiv(program, programInterface, pname, params);
}

void glwGetProgramPipelineInfoLog (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void glwGetProgramPipelineiv (GLuint pipeline, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramPipelineiv(pipeline, pname, params);
}

GLuint glwGetProgramResourceIndex (GLuint program, GLenum programInterface, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->getProgramResourceIndex(program, programInterface, name);
}

GLint glwGetProgramResourceLocation (GLuint program, GLenum programInterface, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getProgramResourceLocation(program, programInterface, name);
}

GLint glwGetProgramResourceLocationIndex (GLuint program, GLenum programInterface, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getProgramResourceLocationIndex(program, programInterface, name);
}

void glwGetProgramResourceName (GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramResourceName(program, programInterface, index, bufSize, length, name);
}

void glwGetProgramResourceiv (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei count, GLsizei *length, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramResourceiv(program, programInterface, index, propCount, props, count, length, params);
}

void glwGetProgramStageiv (GLuint program, GLenum shadertype, GLenum pname, GLint *values)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramStageiv(program, shadertype, pname, values);
}

void glwGetProgramiv (GLuint program, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getProgramiv(program, pname, params);
}

void glwGetQueryBufferObjecti64v (GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryBufferObjecti64v(id, buffer, pname, offset);
}

void glwGetQueryBufferObjectiv (GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryBufferObjectiv(id, buffer, pname, offset);
}

void glwGetQueryBufferObjectui64v (GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryBufferObjectui64v(id, buffer, pname, offset);
}

void glwGetQueryBufferObjectuiv (GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryBufferObjectuiv(id, buffer, pname, offset);
}

void glwGetQueryIndexediv (GLenum target, GLuint index, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryIndexediv(target, index, pname, params);
}

void glwGetQueryObjecti64v (GLuint id, GLenum pname, GLint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryObjecti64v(id, pname, params);
}

void glwGetQueryObjectiv (GLuint id, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryObjectiv(id, pname, params);
}

void glwGetQueryObjectui64v (GLuint id, GLenum pname, GLuint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryObjectui64v(id, pname, params);
}

void glwGetQueryObjectuiv (GLuint id, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryObjectuiv(id, pname, params);
}

void glwGetQueryiv (GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getQueryiv(target, pname, params);
}

void glwGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getRenderbufferParameteriv(target, pname, params);
}

void glwGetSamplerParameterIiv (GLuint sampler, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getSamplerParameterIiv(sampler, pname, params);
}

void glwGetSamplerParameterIuiv (GLuint sampler, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getSamplerParameterIuiv(sampler, pname, params);
}

void glwGetSamplerParameterfv (GLuint sampler, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getSamplerParameterfv(sampler, pname, params);
}

void glwGetSamplerParameteriv (GLuint sampler, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getSamplerParameteriv(sampler, pname, params);
}

void glwGetShaderInfoLog (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getShaderInfoLog(shader, bufSize, length, infoLog);
}

void glwGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void glwGetShaderSource (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getShaderSource(shader, bufSize, length, source);
}

void glwGetShaderiv (GLuint shader, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getShaderiv(shader, pname, params);
}

const GLubyte * glwGetString (GLenum name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (const GLubyte *)0;
    return gl->getString(name);
}

const GLubyte * glwGetStringi (GLenum name, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (const GLubyte *)0;
    return gl->getStringi(name, index);
}

GLuint glwGetSubroutineIndex (GLuint program, GLenum shadertype, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->getSubroutineIndex(program, shadertype, name);
}

GLint glwGetSubroutineUniformLocation (GLuint program, GLenum shadertype, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getSubroutineUniformLocation(program, shadertype, name);
}

void glwGetSynciv (GLsync sync, GLenum pname, GLsizei count, GLsizei *length, GLint *values)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getSynciv(sync, pname, count, length, values);
}

void glwGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexImage(target, level, format, type, pixels);
}

void glwGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexLevelParameterfv(target, level, pname, params);
}

void glwGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexLevelParameteriv(target, level, pname, params);
}

void glwGetTexParameterIiv (GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexParameterIiv(target, pname, params);
}

void glwGetTexParameterIuiv (GLenum target, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexParameterIuiv(target, pname, params);
}

void glwGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexParameterfv(target, pname, params);
}

void glwGetTexParameteriv (GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTexParameteriv(target, pname, params);
}

void glwGetTextureImage (GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureImage(texture, level, format, type, bufSize, pixels);
}

void glwGetTextureImageEXT (GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureImageEXT(texture, target, level, format, type, pixels);
}

void glwGetTextureLevelParameterfv (GLuint texture, GLint level, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureLevelParameterfv(texture, level, pname, params);
}

void glwGetTextureLevelParameterfvEXT (GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureLevelParameterfvEXT(texture, target, level, pname, params);
}

void glwGetTextureLevelParameteriv (GLuint texture, GLint level, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureLevelParameteriv(texture, level, pname, params);
}

void glwGetTextureLevelParameterivEXT (GLuint texture, GLenum target, GLint level, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureLevelParameterivEXT(texture, target, level, pname, params);
}

void glwGetTextureParameterIiv (GLuint texture, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterIiv(texture, pname, params);
}

void glwGetTextureParameterIivEXT (GLuint texture, GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterIivEXT(texture, target, pname, params);
}

void glwGetTextureParameterIuiv (GLuint texture, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterIuiv(texture, pname, params);
}

void glwGetTextureParameterIuivEXT (GLuint texture, GLenum target, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterIuivEXT(texture, target, pname, params);
}

void glwGetTextureParameterfv (GLuint texture, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterfv(texture, pname, params);
}

void glwGetTextureParameterfvEXT (GLuint texture, GLenum target, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterfvEXT(texture, target, pname, params);
}

void glwGetTextureParameteriv (GLuint texture, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameteriv(texture, pname, params);
}

void glwGetTextureParameterivEXT (GLuint texture, GLenum target, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureParameterivEXT(texture, target, pname, params);
}

void glwGetTextureSubImage (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, bufSize, pixels);
}

void glwGetTransformFeedbackVarying (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void glwGetTransformFeedbacki64_v (GLuint xfb, GLenum pname, GLuint index, GLint64 *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTransformFeedbacki64_v(xfb, pname, index, param);
}

void glwGetTransformFeedbacki_v (GLuint xfb, GLenum pname, GLuint index, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTransformFeedbacki_v(xfb, pname, index, param);
}

void glwGetTransformFeedbackiv (GLuint xfb, GLenum pname, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getTransformFeedbackiv(xfb, pname, param);
}

GLuint glwGetUniformBlockIndex (GLuint program, const GLchar *uniformBlockName)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLuint)0;
    return gl->getUniformBlockIndex(program, uniformBlockName);
}

void glwGetUniformIndices (GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

GLint glwGetUniformLocation (GLuint program, const GLchar *name)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLint)0;
    return gl->getUniformLocation(program, name);
}

void glwGetUniformSubroutineuiv (GLenum shadertype, GLint location, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getUniformSubroutineuiv(shadertype, location, params);
}

void glwGetUniformdv (GLuint program, GLint location, GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getUniformdv(program, location, params);
}

void glwGetUniformfv (GLuint program, GLint location, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getUniformfv(program, location, params);
}

void glwGetUniformiv (GLuint program, GLint location, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getUniformiv(program, location, params);
}

void glwGetUniformuiv (GLuint program, GLint location, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getUniformuiv(program, location, params);
}

void glwGetVertexArrayIndexed64iv (GLuint vaobj, GLuint index, GLenum pname, GLint64 *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayIndexed64iv(vaobj, index, pname, param);
}

void glwGetVertexArrayIndexediv (GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayIndexediv(vaobj, index, pname, param);
}

void glwGetVertexArrayIntegeri_vEXT (GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayIntegeri_vEXT(vaobj, index, pname, param);
}

void glwGetVertexArrayIntegervEXT (GLuint vaobj, GLenum pname, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayIntegervEXT(vaobj, pname, param);
}

void glwGetVertexArrayPointeri_vEXT (GLuint vaobj, GLuint index, GLenum pname, void **param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayPointeri_vEXT(vaobj, index, pname, param);
}

void glwGetVertexArrayPointervEXT (GLuint vaobj, GLenum pname, void **param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayPointervEXT(vaobj, pname, param);
}

void glwGetVertexArrayiv (GLuint vaobj, GLenum pname, GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexArrayiv(vaobj, pname, param);
}

void glwGetVertexAttribIiv (GLuint index, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribIiv(index, pname, params);
}

void glwGetVertexAttribIuiv (GLuint index, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribIuiv(index, pname, params);
}

void glwGetVertexAttribLdv (GLuint index, GLenum pname, GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribLdv(index, pname, params);
}

void glwGetVertexAttribPointerv (GLuint index, GLenum pname, void **pointer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribPointerv(index, pname, pointer);
}

void glwGetVertexAttribdv (GLuint index, GLenum pname, GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribdv(index, pname, params);
}

void glwGetVertexAttribfv (GLuint index, GLenum pname, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribfv(index, pname, params);
}

void glwGetVertexAttribiv (GLuint index, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getVertexAttribiv(index, pname, params);
}

void glwGetnCompressedTexImage (GLenum target, GLint lod, GLsizei bufSize, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getnCompressedTexImage(target, lod, bufSize, pixels);
}

void glwGetnTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getnTexImage(target, level, format, type, bufSize, pixels);
}

void glwGetnUniformdv (GLuint program, GLint location, GLsizei bufSize, GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getnUniformdv(program, location, bufSize, params);
}

void glwGetnUniformfv (GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getnUniformfv(program, location, bufSize, params);
}

void glwGetnUniformiv (GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getnUniformiv(program, location, bufSize, params);
}

void glwGetnUniformuiv (GLuint program, GLint location, GLsizei bufSize, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->getnUniformuiv(program, location, bufSize, params);
}

void glwHint (GLenum target, GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->hint(target, mode);
}

void glwInsertEventMarkerEXT (GLsizei length, const GLchar *marker)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->insertEventMarkerEXT(length, marker);
}

void glwInvalidateBufferData (GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateBufferData(buffer);
}

void glwInvalidateBufferSubData (GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateBufferSubData(buffer, offset, length);
}

void glwInvalidateFramebuffer (GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateFramebuffer(target, numAttachments, attachments);
}

void glwInvalidateNamedFramebufferData (GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateNamedFramebufferData(framebuffer, numAttachments, attachments);
}

void glwInvalidateNamedFramebufferSubData (GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateNamedFramebufferSubData(framebuffer, numAttachments, attachments, x, y, width, height);
}

void glwInvalidateSubFramebuffer (GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}

void glwInvalidateTexImage (GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateTexImage(texture, level);
}

void glwInvalidateTexSubImage (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->invalidateTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth);
}

GLboolean glwIsBuffer (GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isBuffer(buffer);
}

GLboolean glwIsEnabled (GLenum cap)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isEnabled(cap);
}

GLboolean glwIsEnabledi (GLenum target, GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isEnabledi(target, index);
}

GLboolean glwIsFramebuffer (GLuint framebuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isFramebuffer(framebuffer);
}

GLboolean glwIsProgram (GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isProgram(program);
}

GLboolean glwIsProgramPipeline (GLuint pipeline)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isProgramPipeline(pipeline);
}

GLboolean glwIsQuery (GLuint id)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isQuery(id);
}

GLboolean glwIsRenderbuffer (GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isRenderbuffer(renderbuffer);
}

GLboolean glwIsSampler (GLuint sampler)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isSampler(sampler);
}

GLboolean glwIsShader (GLuint shader)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isShader(shader);
}

GLboolean glwIsSync (GLsync sync)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isSync(sync);
}

GLboolean glwIsTexture (GLuint texture)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isTexture(texture);
}

GLboolean glwIsTransformFeedback (GLuint id)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isTransformFeedback(id);
}

GLboolean glwIsVertexArray (GLuint array)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->isVertexArray(array);
}

void glwLineWidth (GLfloat width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->lineWidth(width);
}

void glwLinkProgram (GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->linkProgram(program);
}

void glwLogicOp (GLenum opcode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->logicOp(opcode);
}

void * glwMapBuffer (GLenum target, GLenum access)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (void *)0;
    return gl->mapBuffer(target, access);
}

void * glwMapBufferRange (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (void *)0;
    return gl->mapBufferRange(target, offset, length, access);
}

void * glwMapNamedBuffer (GLuint buffer, GLenum access)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (void *)0;
    return gl->mapNamedBuffer(buffer, access);
}

void * glwMapNamedBufferEXT (GLuint buffer, GLenum access)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (void *)0;
    return gl->mapNamedBufferEXT(buffer, access);
}

void * glwMapNamedBufferRange (GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (void *)0;
    return gl->mapNamedBufferRange(buffer, offset, length, access);
}

void * glwMapNamedBufferRangeEXT (GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (void *)0;
    return gl->mapNamedBufferRangeEXT(buffer, offset, length, access);
}

void glwMatrixFrustumEXT (GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixFrustumEXT(mode, left, right, bottom, top, zNear, zFar);
}

void glwMatrixLoadIdentityEXT (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixLoadIdentityEXT(mode);
}

void glwMatrixLoadTransposedEXT (GLenum mode, const GLdouble *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixLoadTransposedEXT(mode, m);
}

void glwMatrixLoadTransposefEXT (GLenum mode, const GLfloat *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixLoadTransposefEXT(mode, m);
}

void glwMatrixLoaddEXT (GLenum mode, const GLdouble *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixLoaddEXT(mode, m);
}

void glwMatrixLoadfEXT (GLenum mode, const GLfloat *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixLoadfEXT(mode, m);
}

void glwMatrixMultTransposedEXT (GLenum mode, const GLdouble *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixMultTransposedEXT(mode, m);
}

void glwMatrixMultTransposefEXT (GLenum mode, const GLfloat *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixMultTransposefEXT(mode, m);
}

void glwMatrixMultdEXT (GLenum mode, const GLdouble *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixMultdEXT(mode, m);
}

void glwMatrixMultfEXT (GLenum mode, const GLfloat *m)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixMultfEXT(mode, m);
}

void glwMatrixOrthoEXT (GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixOrthoEXT(mode, left, right, bottom, top, zNear, zFar);
}

void glwMatrixPopEXT (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixPopEXT(mode);
}

void glwMatrixPushEXT (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixPushEXT(mode);
}

void glwMatrixRotatedEXT (GLenum mode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixRotatedEXT(mode, angle, x, y, z);
}

void glwMatrixRotatefEXT (GLenum mode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixRotatefEXT(mode, angle, x, y, z);
}

void glwMatrixScaledEXT (GLenum mode, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixScaledEXT(mode, x, y, z);
}

void glwMatrixScalefEXT (GLenum mode, GLfloat x, GLfloat y, GLfloat z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixScalefEXT(mode, x, y, z);
}

void glwMatrixTranslatedEXT (GLenum mode, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixTranslatedEXT(mode, x, y, z);
}

void glwMatrixTranslatefEXT (GLenum mode, GLfloat x, GLfloat y, GLfloat z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->matrixTranslatefEXT(mode, x, y, z);
}

void glwMaxShaderCompilerThreadsKHR (GLuint count)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->maxShaderCompilerThreadsKHR(count);
}

void glwMemoryBarrier (GLbitfield barriers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->memoryBarrier(barriers);
}

void glwMemoryBarrierByRegion (GLbitfield barriers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->memoryBarrierByRegion(barriers);
}

void glwMinSampleShading (GLfloat value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->minSampleShading(value);
}

void glwMultiDrawArrays (GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawArrays(mode, first, count, drawcount);
}

void glwMultiDrawArraysIndirect (GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawArraysIndirect(mode, indirect, drawcount, stride);
}

void glwMultiDrawArraysIndirectCount (GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawArraysIndirectCount(mode, indirect, drawcount, maxdrawcount, stride);
}

void glwMultiDrawElements (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawElements(mode, count, type, indices, drawcount);
}

void glwMultiDrawElementsBaseVertex (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
}

void glwMultiDrawElementsIndirect (GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
}

void glwMultiDrawElementsIndirectCount (GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
}

void glwMultiTexBufferEXT (GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexBufferEXT(texunit, target, internalformat, buffer);
}

void glwMultiTexCoordPointerEXT (GLenum texunit, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexCoordPointerEXT(texunit, size, type, stride, pointer);
}

void glwMultiTexEnvfEXT (GLenum texunit, GLenum target, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexEnvfEXT(texunit, target, pname, param);
}

void glwMultiTexEnvfvEXT (GLenum texunit, GLenum target, GLenum pname, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexEnvfvEXT(texunit, target, pname, params);
}

void glwMultiTexEnviEXT (GLenum texunit, GLenum target, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexEnviEXT(texunit, target, pname, param);
}

void glwMultiTexEnvivEXT (GLenum texunit, GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexEnvivEXT(texunit, target, pname, params);
}

void glwMultiTexGendEXT (GLenum texunit, GLenum coord, GLenum pname, GLdouble param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexGendEXT(texunit, coord, pname, param);
}

void glwMultiTexGendvEXT (GLenum texunit, GLenum coord, GLenum pname, const GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexGendvEXT(texunit, coord, pname, params);
}

void glwMultiTexGenfEXT (GLenum texunit, GLenum coord, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexGenfEXT(texunit, coord, pname, param);
}

void glwMultiTexGenfvEXT (GLenum texunit, GLenum coord, GLenum pname, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexGenfvEXT(texunit, coord, pname, params);
}

void glwMultiTexGeniEXT (GLenum texunit, GLenum coord, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexGeniEXT(texunit, coord, pname, param);
}

void glwMultiTexGenivEXT (GLenum texunit, GLenum coord, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexGenivEXT(texunit, coord, pname, params);
}

void glwMultiTexImage1DEXT (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexImage1DEXT(texunit, target, level, internalformat, width, border, format, type, pixels);
}

void glwMultiTexImage2DEXT (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, format, type, pixels);
}

void glwMultiTexImage3DEXT (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void glwMultiTexParameterIivEXT (GLenum texunit, GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexParameterIivEXT(texunit, target, pname, params);
}

void glwMultiTexParameterIuivEXT (GLenum texunit, GLenum target, GLenum pname, const GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexParameterIuivEXT(texunit, target, pname, params);
}

void glwMultiTexParameterfEXT (GLenum texunit, GLenum target, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexParameterfEXT(texunit, target, pname, param);
}

void glwMultiTexParameterfvEXT (GLenum texunit, GLenum target, GLenum pname, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexParameterfvEXT(texunit, target, pname, params);
}

void glwMultiTexParameteriEXT (GLenum texunit, GLenum target, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexParameteriEXT(texunit, target, pname, param);
}

void glwMultiTexParameterivEXT (GLenum texunit, GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexParameterivEXT(texunit, target, pname, params);
}

void glwMultiTexRenderbufferEXT (GLenum texunit, GLenum target, GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexRenderbufferEXT(texunit, target, renderbuffer);
}

void glwMultiTexSubImage1DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, type, pixels);
}

void glwMultiTexSubImage2DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glwMultiTexSubImage3DEXT (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void glwMulticastBarrierNV (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastBarrierNV();
}

void glwMulticastBlitFramebufferNV (GLuint srcGpu, GLuint dstGpu, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastBlitFramebufferNV(srcGpu, dstGpu, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void glwMulticastBufferSubDataNV (GLbitfield gpuMask, GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastBufferSubDataNV(gpuMask, buffer, offset, size, data);
}

void glwMulticastCopyBufferSubDataNV (GLuint readGpu, GLbitfield writeGpuMask, GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastCopyBufferSubDataNV(readGpu, writeGpuMask, readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void glwMulticastCopyImageSubDataNV (GLuint srcGpu, GLbitfield dstGpuMask, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastCopyImageSubDataNV(srcGpu, dstGpuMask, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void glwMulticastFramebufferSampleLocationsfvNV (GLuint gpu, GLuint framebuffer, GLuint start, GLsizei count, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastFramebufferSampleLocationsfvNV(gpu, framebuffer, start, count, v);
}

void glwMulticastGetQueryObjecti64vNV (GLuint gpu, GLuint id, GLenum pname, GLint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastGetQueryObjecti64vNV(gpu, id, pname, params);
}

void glwMulticastGetQueryObjectivNV (GLuint gpu, GLuint id, GLenum pname, GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastGetQueryObjectivNV(gpu, id, pname, params);
}

void glwMulticastGetQueryObjectui64vNV (GLuint gpu, GLuint id, GLenum pname, GLuint64 *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastGetQueryObjectui64vNV(gpu, id, pname, params);
}

void glwMulticastGetQueryObjectuivNV (GLuint gpu, GLuint id, GLenum pname, GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastGetQueryObjectuivNV(gpu, id, pname, params);
}

void glwMulticastWaitSyncNV (GLuint signalGpu, GLbitfield waitGpuMask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->multicastWaitSyncNV(signalGpu, waitGpuMask);
}

void glwNamedBufferData (GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedBufferData(buffer, size, data, usage);
}

void glwNamedBufferDataEXT (GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedBufferDataEXT(buffer, size, data, usage);
}

void glwNamedBufferPageCommitmentARB (GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedBufferPageCommitmentARB(buffer, offset, size, commit);
}

void glwNamedBufferPageCommitmentEXT (GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedBufferPageCommitmentEXT(buffer, offset, size, commit);
}

void glwNamedBufferStorage (GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedBufferStorage(buffer, size, data, flags);
}

void glwNamedBufferSubData (GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedBufferSubData(buffer, offset, size, data);
}

void glwNamedCopyBufferSubDataEXT (GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void glwNamedFramebufferDrawBuffer (GLuint framebuffer, GLenum buf)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferDrawBuffer(framebuffer, buf);
}

void glwNamedFramebufferDrawBuffers (GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferDrawBuffers(framebuffer, n, bufs);
}

void glwNamedFramebufferParameteri (GLuint framebuffer, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferParameteri(framebuffer, pname, param);
}

void glwNamedFramebufferParameteriEXT (GLuint framebuffer, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferParameteriEXT(framebuffer, pname, param);
}

void glwNamedFramebufferReadBuffer (GLuint framebuffer, GLenum src)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferReadBuffer(framebuffer, src);
}

void glwNamedFramebufferRenderbuffer (GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferRenderbuffer(framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void glwNamedFramebufferRenderbufferEXT (GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void glwNamedFramebufferTexture (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTexture(framebuffer, attachment, texture, level);
}

void glwNamedFramebufferTexture1DEXT (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);
}

void glwNamedFramebufferTexture2DEXT (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
}

void glwNamedFramebufferTexture3DEXT (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level, zoffset);
}

void glwNamedFramebufferTextureEXT (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTextureEXT(framebuffer, attachment, texture, level);
}

void glwNamedFramebufferTextureFaceEXT (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLenum face)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTextureFaceEXT(framebuffer, attachment, texture, level, face);
}

void glwNamedFramebufferTextureLayer (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTextureLayer(framebuffer, attachment, texture, level, layer);
}

void glwNamedFramebufferTextureLayerEXT (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
}

void glwNamedFramebufferTextureMultiviewOVR (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedFramebufferTextureMultiviewOVR(framebuffer, attachment, texture, level, baseViewIndex, numViews);
}

void glwNamedProgramLocalParameter4dEXT (GLuint program, GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameter4dEXT(program, target, index, x, y, z, w);
}

void glwNamedProgramLocalParameter4dvEXT (GLuint program, GLenum target, GLuint index, const GLdouble *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameter4dvEXT(program, target, index, params);
}

void glwNamedProgramLocalParameter4fEXT (GLuint program, GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameter4fEXT(program, target, index, x, y, z, w);
}

void glwNamedProgramLocalParameter4fvEXT (GLuint program, GLenum target, GLuint index, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameter4fvEXT(program, target, index, params);
}

void glwNamedProgramLocalParameterI4iEXT (GLuint program, GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameterI4iEXT(program, target, index, x, y, z, w);
}

void glwNamedProgramLocalParameterI4ivEXT (GLuint program, GLenum target, GLuint index, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameterI4ivEXT(program, target, index, params);
}

void glwNamedProgramLocalParameterI4uiEXT (GLuint program, GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameterI4uiEXT(program, target, index, x, y, z, w);
}

void glwNamedProgramLocalParameterI4uivEXT (GLuint program, GLenum target, GLuint index, const GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameterI4uivEXT(program, target, index, params);
}

void glwNamedProgramLocalParameters4fvEXT (GLuint program, GLenum target, GLuint index, GLsizei count, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParameters4fvEXT(program, target, index, count, params);
}

void glwNamedProgramLocalParametersI4ivEXT (GLuint program, GLenum target, GLuint index, GLsizei count, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParametersI4ivEXT(program, target, index, count, params);
}

void glwNamedProgramLocalParametersI4uivEXT (GLuint program, GLenum target, GLuint index, GLsizei count, const GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramLocalParametersI4uivEXT(program, target, index, count, params);
}

void glwNamedProgramStringEXT (GLuint program, GLenum target, GLenum format, GLsizei len, const void *string)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedProgramStringEXT(program, target, format, len, string);
}

void glwNamedRenderbufferStorage (GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedRenderbufferStorage(renderbuffer, internalformat, width, height);
}

void glwNamedRenderbufferStorageEXT (GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedRenderbufferStorageEXT(renderbuffer, internalformat, width, height);
}

void glwNamedRenderbufferStorageMultisample (GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedRenderbufferStorageMultisample(renderbuffer, samples, internalformat, width, height);
}

void glwNamedRenderbufferStorageMultisampleCoverageEXT (GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedRenderbufferStorageMultisampleCoverageEXT(renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
}

void glwNamedRenderbufferStorageMultisampleEXT (GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->namedRenderbufferStorageMultisampleEXT(renderbuffer, samples, internalformat, width, height);
}

void glwObjectLabel (GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->objectLabel(identifier, name, length, label);
}

void glwObjectPtrLabel (const void *ptr, GLsizei length, const GLchar *label)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->objectPtrLabel(ptr, length, label);
}

void glwPatchParameterfv (GLenum pname, const GLfloat *values)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->patchParameterfv(pname, values);
}

void glwPatchParameteri (GLenum pname, GLint value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->patchParameteri(pname, value);
}

void glwPauseTransformFeedback (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pauseTransformFeedback();
}

void glwPixelStoref (GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pixelStoref(pname, param);
}

void glwPixelStorei (GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pixelStorei(pname, param);
}

void glwPointParameterf (GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pointParameterf(pname, param);
}

void glwPointParameterfv (GLenum pname, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pointParameterfv(pname, params);
}

void glwPointParameteri (GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pointParameteri(pname, param);
}

void glwPointParameteriv (GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pointParameteriv(pname, params);
}

void glwPointSize (GLfloat size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pointSize(size);
}

void glwPolygonMode (GLenum face, GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->polygonMode(face, mode);
}

void glwPolygonOffset (GLfloat factor, GLfloat units)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->polygonOffset(factor, units);
}

void glwPolygonOffsetClamp (GLfloat factor, GLfloat units, GLfloat clamp)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->polygonOffsetClamp(factor, units, clamp);
}

void glwPopDebugGroup (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->popDebugGroup();
}

void glwPopGroupMarkerEXT (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->popGroupMarkerEXT();
}

void glwPrimitiveBoundingBox (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->primitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
}

void glwPrimitiveRestartIndex (GLuint index)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->primitiveRestartIndex(index);
}

void glwProgramBinary (GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programBinary(program, binaryFormat, binary, length);
}

void glwProgramParameteri (GLuint program, GLenum pname, GLint value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programParameteri(program, pname, value);
}

void glwProgramUniform1d (GLuint program, GLint location, GLdouble v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1d(program, location, v0);
}

void glwProgramUniform1dEXT (GLuint program, GLint location, GLdouble x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1dEXT(program, location, x);
}

void glwProgramUniform1dv (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1dv(program, location, count, value);
}

void glwProgramUniform1dvEXT (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1dvEXT(program, location, count, value);
}

void glwProgramUniform1f (GLuint program, GLint location, GLfloat v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1f(program, location, v0);
}

void glwProgramUniform1fv (GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1fv(program, location, count, value);
}

void glwProgramUniform1i (GLuint program, GLint location, GLint v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1i(program, location, v0);
}

void glwProgramUniform1iv (GLuint program, GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1iv(program, location, count, value);
}

void glwProgramUniform1ui (GLuint program, GLint location, GLuint v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1ui(program, location, v0);
}

void glwProgramUniform1uiv (GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform1uiv(program, location, count, value);
}

void glwProgramUniform2d (GLuint program, GLint location, GLdouble v0, GLdouble v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2d(program, location, v0, v1);
}

void glwProgramUniform2dEXT (GLuint program, GLint location, GLdouble x, GLdouble y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2dEXT(program, location, x, y);
}

void glwProgramUniform2dv (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2dv(program, location, count, value);
}

void glwProgramUniform2dvEXT (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2dvEXT(program, location, count, value);
}

void glwProgramUniform2f (GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2f(program, location, v0, v1);
}

void glwProgramUniform2fv (GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2fv(program, location, count, value);
}

void glwProgramUniform2i (GLuint program, GLint location, GLint v0, GLint v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2i(program, location, v0, v1);
}

void glwProgramUniform2iv (GLuint program, GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2iv(program, location, count, value);
}

void glwProgramUniform2ui (GLuint program, GLint location, GLuint v0, GLuint v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2ui(program, location, v0, v1);
}

void glwProgramUniform2uiv (GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform2uiv(program, location, count, value);
}

void glwProgramUniform3d (GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3d(program, location, v0, v1, v2);
}

void glwProgramUniform3dEXT (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3dEXT(program, location, x, y, z);
}

void glwProgramUniform3dv (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3dv(program, location, count, value);
}

void glwProgramUniform3dvEXT (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3dvEXT(program, location, count, value);
}

void glwProgramUniform3f (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3f(program, location, v0, v1, v2);
}

void glwProgramUniform3fv (GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3fv(program, location, count, value);
}

void glwProgramUniform3i (GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3i(program, location, v0, v1, v2);
}

void glwProgramUniform3iv (GLuint program, GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3iv(program, location, count, value);
}

void glwProgramUniform3ui (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3ui(program, location, v0, v1, v2);
}

void glwProgramUniform3uiv (GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform3uiv(program, location, count, value);
}

void glwProgramUniform4d (GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4d(program, location, v0, v1, v2, v3);
}

void glwProgramUniform4dEXT (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4dEXT(program, location, x, y, z, w);
}

void glwProgramUniform4dv (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4dv(program, location, count, value);
}

void glwProgramUniform4dvEXT (GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4dvEXT(program, location, count, value);
}

void glwProgramUniform4f (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4f(program, location, v0, v1, v2, v3);
}

void glwProgramUniform4fv (GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4fv(program, location, count, value);
}

void glwProgramUniform4i (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4i(program, location, v0, v1, v2, v3);
}

void glwProgramUniform4iv (GLuint program, GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4iv(program, location, count, value);
}

void glwProgramUniform4ui (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4ui(program, location, v0, v1, v2, v3);
}

void glwProgramUniform4uiv (GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniform4uiv(program, location, count, value);
}

void glwProgramUniformMatrix2dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2x3dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2x3dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2x3dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2x3dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2x3fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2x3fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2x4dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2x4dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2x4dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2x4dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix2x4fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix2x4fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3x2dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3x2dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3x2dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3x2dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3x2fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3x2fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3x4dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3x4dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3x4dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3x4dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix3x4fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix3x4fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4x2dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4x2dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4x2dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4x2dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4x2fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4x2fv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4x3dv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4x3dv(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4x3dvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4x3dvEXT(program, location, count, transpose, value);
}

void glwProgramUniformMatrix4x3fv (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->programUniformMatrix4x3fv(program, location, count, transpose, value);
}

void glwProvokingVertex (GLenum mode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->provokingVertex(mode);
}

void glwPushClientAttribDefaultEXT (GLbitfield mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pushClientAttribDefaultEXT(mask);
}

void glwPushDebugGroup (GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pushDebugGroup(source, id, length, message);
}

void glwPushGroupMarkerEXT (GLsizei length, const GLchar *marker)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->pushGroupMarkerEXT(length, marker);
}

void glwQueryCounter (GLuint id, GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->queryCounter(id, target);
}

void glwReadBuffer (GLenum src)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->readBuffer(src);
}

void glwReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->readPixels(x, y, width, height, format, type, pixels);
}

void glwReadnPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->readnPixels(x, y, width, height, format, type, bufSize, data);
}

void glwReleaseShaderCompiler (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->releaseShaderCompiler();
}

void glwRenderGpuMaskNV (GLbitfield mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->renderGpuMaskNV(mask);
}

void glwRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->renderbufferStorage(target, internalformat, width, height);
}

void glwRenderbufferStorageMultisample (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->renderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void glwRenderbufferStorageMultisampleEXT (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->renderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
}

void glwResumeTransformFeedback (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->resumeTransformFeedback();
}

void glwSampleCoverage (GLfloat value, GLboolean invert)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->sampleCoverage(value, invert);
}

void glwSampleMaski (GLuint maskNumber, GLbitfield mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->sampleMaski(maskNumber, mask);
}

void glwSamplerParameterIiv (GLuint sampler, GLenum pname, const GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->samplerParameterIiv(sampler, pname, param);
}

void glwSamplerParameterIuiv (GLuint sampler, GLenum pname, const GLuint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->samplerParameterIuiv(sampler, pname, param);
}

void glwSamplerParameterf (GLuint sampler, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->samplerParameterf(sampler, pname, param);
}

void glwSamplerParameterfv (GLuint sampler, GLenum pname, const GLfloat *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->samplerParameterfv(sampler, pname, param);
}

void glwSamplerParameteri (GLuint sampler, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->samplerParameteri(sampler, pname, param);
}

void glwSamplerParameteriv (GLuint sampler, GLenum pname, const GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->samplerParameteriv(sampler, pname, param);
}

void glwScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->scissor(x, y, width, height);
}

void glwScissorArrayv (GLuint first, GLsizei count, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->scissorArrayv(first, count, v);
}

void glwScissorIndexed (GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->scissorIndexed(index, left, bottom, width, height);
}

void glwScissorIndexedv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->scissorIndexedv(index, v);
}

void glwShaderBinary (GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->shaderBinary(count, shaders, binaryFormat, binary, length);
}

void glwShaderSource (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->shaderSource(shader, count, string, length);
}

void glwShaderStorageBlockBinding (GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->shaderStorageBlockBinding(program, storageBlockIndex, storageBlockBinding);
}

void glwShadingRateEXT (GLenum rate)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->shadingRateEXT(rate);
}

void glwShadingRateCombinerOpsEXT (GLenum combinerOp0, GLenum combinerOp1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->shadingRateCombinerOpsEXT(combinerOp0, combinerOp1);
}

void glwSpecializeShader (GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->specializeShader(shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

void glwStencilFunc (GLenum func, GLint ref, GLuint mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->stencilFunc(func, ref, mask);
}

void glwStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->stencilFuncSeparate(face, func, ref, mask);
}

void glwStencilMask (GLuint mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->stencilMask(mask);
}

void glwStencilMaskSeparate (GLenum face, GLuint mask)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->stencilMaskSeparate(face, mask);
}

void glwStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->stencilOp(fail, zfail, zpass);
}

void glwStencilOpSeparate (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->stencilOpSeparate(face, sfail, dpfail, dppass);
}

void glwTexBuffer (GLenum target, GLenum internalformat, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texBuffer(target, internalformat, buffer);
}

void glwTexBufferRange (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texBufferRange(target, internalformat, buffer, offset, size);
}

void glwTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texImage1D(target, level, internalformat, width, border, format, type, pixels);
}

void glwTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void glwTexImage2DMultisample (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texImage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
}

void glwTexImage3D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void glwTexImage3DMultisample (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texImage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void glwTexImage3DOES (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texImage3DOES(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void glwTexPageCommitmentARB (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texPageCommitmentARB(target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void glwTexParameterIiv (GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texParameterIiv(target, pname, params);
}

void glwTexParameterIuiv (GLenum target, GLenum pname, const GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texParameterIuiv(target, pname, params);
}

void glwTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texParameterf(target, pname, param);
}

void glwTexParameterfv (GLenum target, GLenum pname, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texParameterfv(target, pname, params);
}

void glwTexParameteri (GLenum target, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texParameteri(target, pname, param);
}

void glwTexParameteriv (GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texParameteriv(target, pname, params);
}

void glwTexStorage1D (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texStorage1D(target, levels, internalformat, width);
}

void glwTexStorage2D (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texStorage2D(target, levels, internalformat, width, height);
}

void glwTexStorage2DMultisample (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texStorage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
}

void glwTexStorage3D (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texStorage3D(target, levels, internalformat, width, height, depth);
}

void glwTexStorage3DMultisample (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texStorage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void glwTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texSubImage1D(target, level, xoffset, width, format, type, pixels);
}

void glwTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glwTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void glwTexSubImage3DOES (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void glwTextureBarrier (void)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureBarrier();
}

void glwTextureBuffer (GLuint texture, GLenum internalformat, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureBuffer(texture, internalformat, buffer);
}

void glwTextureBufferEXT (GLuint texture, GLenum target, GLenum internalformat, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureBufferEXT(texture, target, internalformat, buffer);
}

void glwTextureBufferRange (GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureBufferRange(texture, internalformat, buffer, offset, size);
}

void glwTextureBufferRangeEXT (GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureBufferRangeEXT(texture, target, internalformat, buffer, offset, size);
}

void glwTextureImage1DEXT (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureImage1DEXT(texture, target, level, internalformat, width, border, format, type, pixels);
}

void glwTextureImage2DEXT (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureImage2DEXT(texture, target, level, internalformat, width, height, border, format, type, pixels);
}

void glwTextureImage3DEXT (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void glwTexturePageCommitmentEXT (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->texturePageCommitmentEXT(texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void glwTextureParameterIiv (GLuint texture, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterIiv(texture, pname, params);
}

void glwTextureParameterIivEXT (GLuint texture, GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterIivEXT(texture, target, pname, params);
}

void glwTextureParameterIuiv (GLuint texture, GLenum pname, const GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterIuiv(texture, pname, params);
}

void glwTextureParameterIuivEXT (GLuint texture, GLenum target, GLenum pname, const GLuint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterIuivEXT(texture, target, pname, params);
}

void glwTextureParameterf (GLuint texture, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterf(texture, pname, param);
}

void glwTextureParameterfEXT (GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterfEXT(texture, target, pname, param);
}

void glwTextureParameterfv (GLuint texture, GLenum pname, const GLfloat *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterfv(texture, pname, param);
}

void glwTextureParameterfvEXT (GLuint texture, GLenum target, GLenum pname, const GLfloat *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterfvEXT(texture, target, pname, params);
}

void glwTextureParameteri (GLuint texture, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameteri(texture, pname, param);
}

void glwTextureParameteriEXT (GLuint texture, GLenum target, GLenum pname, GLint param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameteriEXT(texture, target, pname, param);
}

void glwTextureParameteriv (GLuint texture, GLenum pname, const GLint *param)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameteriv(texture, pname, param);
}

void glwTextureParameterivEXT (GLuint texture, GLenum target, GLenum pname, const GLint *params)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureParameterivEXT(texture, target, pname, params);
}

void glwTextureRenderbufferEXT (GLuint texture, GLenum target, GLuint renderbuffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureRenderbufferEXT(texture, target, renderbuffer);
}

void glwTextureStorage1D (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage1D(texture, levels, internalformat, width);
}

void glwTextureStorage1DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage1DEXT(texture, target, levels, internalformat, width);
}

void glwTextureStorage2D (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage2D(texture, levels, internalformat, width, height);
}

void glwTextureStorage2DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage2DEXT(texture, target, levels, internalformat, width, height);
}

void glwTextureStorage2DMultisample (GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage2DMultisample(texture, samples, internalformat, width, height, fixedsamplelocations);
}

void glwTextureStorage2DMultisampleEXT (GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage2DMultisampleEXT(texture, target, samples, internalformat, width, height, fixedsamplelocations);
}

void glwTextureStorage3D (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage3D(texture, levels, internalformat, width, height, depth);
}

void glwTextureStorage3DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
}

void glwTextureStorage3DMultisample (GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage3DMultisample(texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void glwTextureStorage3DMultisampleEXT (GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureStorage3DMultisampleEXT(texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void glwTextureSubImage1D (GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureSubImage1D(texture, level, xoffset, width, format, type, pixels);
}

void glwTextureSubImage1DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels);
}

void glwTextureSubImage2D (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glwTextureSubImage2DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glwTextureSubImage3D (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void glwTextureSubImage3DEXT (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void glwTextureView (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->textureView(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

void glwTransformFeedbackBufferBase (GLuint xfb, GLuint index, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->transformFeedbackBufferBase(xfb, index, buffer);
}

void glwTransformFeedbackBufferRange (GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->transformFeedbackBufferRange(xfb, index, buffer, offset, size);
}

void glwTransformFeedbackVaryings (GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->transformFeedbackVaryings(program, count, varyings, bufferMode);
}

void glwUniform1d (GLint location, GLdouble x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1d(location, x);
}

void glwUniform1dv (GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1dv(location, count, value);
}

void glwUniform1f (GLint location, GLfloat v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1f(location, v0);
}

void glwUniform1fv (GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1fv(location, count, value);
}

void glwUniform1i (GLint location, GLint v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1i(location, v0);
}

void glwUniform1iv (GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1iv(location, count, value);
}

void glwUniform1ui (GLint location, GLuint v0)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1ui(location, v0);
}

void glwUniform1uiv (GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform1uiv(location, count, value);
}

void glwUniform2d (GLint location, GLdouble x, GLdouble y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2d(location, x, y);
}

void glwUniform2dv (GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2dv(location, count, value);
}

void glwUniform2f (GLint location, GLfloat v0, GLfloat v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2f(location, v0, v1);
}

void glwUniform2fv (GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2fv(location, count, value);
}

void glwUniform2i (GLint location, GLint v0, GLint v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2i(location, v0, v1);
}

void glwUniform2iv (GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2iv(location, count, value);
}

void glwUniform2ui (GLint location, GLuint v0, GLuint v1)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2ui(location, v0, v1);
}

void glwUniform2uiv (GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform2uiv(location, count, value);
}

void glwUniform3d (GLint location, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3d(location, x, y, z);
}

void glwUniform3dv (GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3dv(location, count, value);
}

void glwUniform3f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3f(location, v0, v1, v2);
}

void glwUniform3fv (GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3fv(location, count, value);
}

void glwUniform3i (GLint location, GLint v0, GLint v1, GLint v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3i(location, v0, v1, v2);
}

void glwUniform3iv (GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3iv(location, count, value);
}

void glwUniform3ui (GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3ui(location, v0, v1, v2);
}

void glwUniform3uiv (GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform3uiv(location, count, value);
}

void glwUniform4d (GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4d(location, x, y, z, w);
}

void glwUniform4dv (GLint location, GLsizei count, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4dv(location, count, value);
}

void glwUniform4f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4f(location, v0, v1, v2, v3);
}

void glwUniform4fv (GLint location, GLsizei count, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4fv(location, count, value);
}

void glwUniform4i (GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4i(location, v0, v1, v2, v3);
}

void glwUniform4iv (GLint location, GLsizei count, const GLint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4iv(location, count, value);
}

void glwUniform4ui (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4ui(location, v0, v1, v2, v3);
}

void glwUniform4uiv (GLint location, GLsizei count, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniform4uiv(location, count, value);
}

void glwUniformBlockBinding (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

void glwUniformMatrix2dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix2dv(location, count, transpose, value);
}

void glwUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix2fv(location, count, transpose, value);
}

void glwUniformMatrix2x3dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix2x3dv(location, count, transpose, value);
}

void glwUniformMatrix2x3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix2x3fv(location, count, transpose, value);
}

void glwUniformMatrix2x4dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix2x4dv(location, count, transpose, value);
}

void glwUniformMatrix2x4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix2x4fv(location, count, transpose, value);
}

void glwUniformMatrix3dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix3dv(location, count, transpose, value);
}

void glwUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix3fv(location, count, transpose, value);
}

void glwUniformMatrix3x2dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix3x2dv(location, count, transpose, value);
}

void glwUniformMatrix3x2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix3x2fv(location, count, transpose, value);
}

void glwUniformMatrix3x4dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix3x4dv(location, count, transpose, value);
}

void glwUniformMatrix3x4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix3x4fv(location, count, transpose, value);
}

void glwUniformMatrix4dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix4dv(location, count, transpose, value);
}

void glwUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix4fv(location, count, transpose, value);
}

void glwUniformMatrix4x2dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix4x2dv(location, count, transpose, value);
}

void glwUniformMatrix4x2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix4x2fv(location, count, transpose, value);
}

void glwUniformMatrix4x3dv (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix4x3dv(location, count, transpose, value);
}

void glwUniformMatrix4x3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformMatrix4x3fv(location, count, transpose, value);
}

void glwUniformSubroutinesuiv (GLenum shadertype, GLsizei count, const GLuint *indices)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->uniformSubroutinesuiv(shadertype, count, indices);
}

GLboolean glwUnmapBuffer (GLenum target)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->unmapBuffer(target);
}

GLboolean glwUnmapNamedBuffer (GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->unmapNamedBuffer(buffer);
}

GLboolean glwUnmapNamedBufferEXT (GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return (GLboolean)0;
    return gl->unmapNamedBufferEXT(buffer);
}

void glwUseProgram (GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->useProgram(program);
}

void glwUseProgramStages (GLuint pipeline, GLbitfield stages, GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->useProgramStages(pipeline, stages, program);
}

void glwValidateProgram (GLuint program)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->validateProgram(program);
}

void glwValidateProgramPipeline (GLuint pipeline)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->validateProgramPipeline(pipeline);
}

void glwVertexArrayAttribBinding (GLuint vaobj, GLuint attribindex, GLuint bindingindex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayAttribBinding(vaobj, attribindex, bindingindex);
}

void glwVertexArrayAttribFormat (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayAttribFormat(vaobj, attribindex, size, type, normalized, relativeoffset);
}

void glwVertexArrayAttribIFormat (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayAttribIFormat(vaobj, attribindex, size, type, relativeoffset);
}

void glwVertexArrayAttribLFormat (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayAttribLFormat(vaobj, attribindex, size, type, relativeoffset);
}

void glwVertexArrayBindVertexBufferEXT (GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride);
}

void glwVertexArrayBindingDivisor (GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayBindingDivisor(vaobj, bindingindex, divisor);
}

void glwVertexArrayColorOffsetEXT (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayColorOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void glwVertexArrayEdgeFlagOffsetEXT (GLuint vaobj, GLuint buffer, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayEdgeFlagOffsetEXT(vaobj, buffer, stride, offset);
}

void glwVertexArrayElementBuffer (GLuint vaobj, GLuint buffer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayElementBuffer(vaobj, buffer);
}

void glwVertexArrayFogCoordOffsetEXT (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayFogCoordOffsetEXT(vaobj, buffer, type, stride, offset);
}

void glwVertexArrayIndexOffsetEXT (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayIndexOffsetEXT(vaobj, buffer, type, stride, offset);
}

void glwVertexArrayMultiTexCoordOffsetEXT (GLuint vaobj, GLuint buffer, GLenum texunit, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayMultiTexCoordOffsetEXT(vaobj, buffer, texunit, size, type, stride, offset);
}

void glwVertexArrayNormalOffsetEXT (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayNormalOffsetEXT(vaobj, buffer, type, stride, offset);
}

void glwVertexArraySecondaryColorOffsetEXT (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArraySecondaryColorOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void glwVertexArrayTexCoordOffsetEXT (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayTexCoordOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void glwVertexArrayVertexAttribBindingEXT (GLuint vaobj, GLuint attribindex, GLuint bindingindex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribBindingEXT(vaobj, attribindex, bindingindex);
}

void glwVertexArrayVertexAttribDivisorEXT (GLuint vaobj, GLuint index, GLuint divisor)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribDivisorEXT(vaobj, index, divisor);
}

void glwVertexArrayVertexAttribFormatEXT (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type, normalized, relativeoffset);
}

void glwVertexArrayVertexAttribIFormatEXT (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset);
}

void glwVertexArrayVertexAttribIOffsetEXT (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
}

void glwVertexArrayVertexAttribLFormatEXT (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset);
}

void glwVertexArrayVertexAttribLOffsetEXT (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
}

void glwVertexArrayVertexAttribOffsetEXT (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type, normalized, stride, offset);
}

void glwVertexArrayVertexBindingDivisorEXT (GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor);
}

void glwVertexArrayVertexBuffer (GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexBuffer(vaobj, bindingindex, buffer, offset, stride);
}

void glwVertexArrayVertexBuffers (GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexBuffers(vaobj, first, count, buffers, offsets, strides);
}

void glwVertexArrayVertexOffsetEXT (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexArrayVertexOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void glwVertexAttrib1d (GLuint index, GLdouble x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib1d(index, x);
}

void glwVertexAttrib1dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib1dv(index, v);
}

void glwVertexAttrib1f (GLuint index, GLfloat x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib1f(index, x);
}

void glwVertexAttrib1fv (GLuint index, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib1fv(index, v);
}

void glwVertexAttrib1s (GLuint index, GLshort x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib1s(index, x);
}

void glwVertexAttrib1sv (GLuint index, const GLshort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib1sv(index, v);
}

void glwVertexAttrib2d (GLuint index, GLdouble x, GLdouble y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib2d(index, x, y);
}

void glwVertexAttrib2dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib2dv(index, v);
}

void glwVertexAttrib2f (GLuint index, GLfloat x, GLfloat y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib2f(index, x, y);
}

void glwVertexAttrib2fv (GLuint index, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib2fv(index, v);
}

void glwVertexAttrib2s (GLuint index, GLshort x, GLshort y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib2s(index, x, y);
}

void glwVertexAttrib2sv (GLuint index, const GLshort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib2sv(index, v);
}

void glwVertexAttrib3d (GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib3d(index, x, y, z);
}

void glwVertexAttrib3dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib3dv(index, v);
}

void glwVertexAttrib3f (GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib3f(index, x, y, z);
}

void glwVertexAttrib3fv (GLuint index, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib3fv(index, v);
}

void glwVertexAttrib3s (GLuint index, GLshort x, GLshort y, GLshort z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib3s(index, x, y, z);
}

void glwVertexAttrib3sv (GLuint index, const GLshort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib3sv(index, v);
}

void glwVertexAttrib4Nbv (GLuint index, const GLbyte *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Nbv(index, v);
}

void glwVertexAttrib4Niv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Niv(index, v);
}

void glwVertexAttrib4Nsv (GLuint index, const GLshort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Nsv(index, v);
}

void glwVertexAttrib4Nub (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Nub(index, x, y, z, w);
}

void glwVertexAttrib4Nubv (GLuint index, const GLubyte *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Nubv(index, v);
}

void glwVertexAttrib4Nuiv (GLuint index, const GLuint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Nuiv(index, v);
}

void glwVertexAttrib4Nusv (GLuint index, const GLushort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4Nusv(index, v);
}

void glwVertexAttrib4bv (GLuint index, const GLbyte *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4bv(index, v);
}

void glwVertexAttrib4d (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4d(index, x, y, z, w);
}

void glwVertexAttrib4dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4dv(index, v);
}

void glwVertexAttrib4f (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4f(index, x, y, z, w);
}

void glwVertexAttrib4fv (GLuint index, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4fv(index, v);
}

void glwVertexAttrib4iv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4iv(index, v);
}

void glwVertexAttrib4s (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4s(index, x, y, z, w);
}

void glwVertexAttrib4sv (GLuint index, const GLshort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4sv(index, v);
}

void glwVertexAttrib4ubv (GLuint index, const GLubyte *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4ubv(index, v);
}

void glwVertexAttrib4uiv (GLuint index, const GLuint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4uiv(index, v);
}

void glwVertexAttrib4usv (GLuint index, const GLushort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttrib4usv(index, v);
}

void glwVertexAttribBinding (GLuint attribindex, GLuint bindingindex)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribBinding(attribindex, bindingindex);
}

void glwVertexAttribDivisor (GLuint index, GLuint divisor)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribDivisor(index, divisor);
}

void glwVertexAttribFormat (GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

void glwVertexAttribI1i (GLuint index, GLint x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI1i(index, x);
}

void glwVertexAttribI1iv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI1iv(index, v);
}

void glwVertexAttribI1ui (GLuint index, GLuint x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI1ui(index, x);
}

void glwVertexAttribI1uiv (GLuint index, const GLuint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI1uiv(index, v);
}

void glwVertexAttribI2i (GLuint index, GLint x, GLint y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI2i(index, x, y);
}

void glwVertexAttribI2iv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI2iv(index, v);
}

void glwVertexAttribI2ui (GLuint index, GLuint x, GLuint y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI2ui(index, x, y);
}

void glwVertexAttribI2uiv (GLuint index, const GLuint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI2uiv(index, v);
}

void glwVertexAttribI3i (GLuint index, GLint x, GLint y, GLint z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI3i(index, x, y, z);
}

void glwVertexAttribI3iv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI3iv(index, v);
}

void glwVertexAttribI3ui (GLuint index, GLuint x, GLuint y, GLuint z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI3ui(index, x, y, z);
}

void glwVertexAttribI3uiv (GLuint index, const GLuint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI3uiv(index, v);
}

void glwVertexAttribI4bv (GLuint index, const GLbyte *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4bv(index, v);
}

void glwVertexAttribI4i (GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4i(index, x, y, z, w);
}

void glwVertexAttribI4iv (GLuint index, const GLint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4iv(index, v);
}

void glwVertexAttribI4sv (GLuint index, const GLshort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4sv(index, v);
}

void glwVertexAttribI4ubv (GLuint index, const GLubyte *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4ubv(index, v);
}

void glwVertexAttribI4ui (GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4ui(index, x, y, z, w);
}

void glwVertexAttribI4uiv (GLuint index, const GLuint *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4uiv(index, v);
}

void glwVertexAttribI4usv (GLuint index, const GLushort *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribI4usv(index, v);
}

void glwVertexAttribIFormat (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribIFormat(attribindex, size, type, relativeoffset);
}

void glwVertexAttribIPointer (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribIPointer(index, size, type, stride, pointer);
}

void glwVertexAttribL1d (GLuint index, GLdouble x)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL1d(index, x);
}

void glwVertexAttribL1dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL1dv(index, v);
}

void glwVertexAttribL2d (GLuint index, GLdouble x, GLdouble y)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL2d(index, x, y);
}

void glwVertexAttribL2dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL2dv(index, v);
}

void glwVertexAttribL3d (GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL3d(index, x, y, z);
}

void glwVertexAttribL3dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL3dv(index, v);
}

void glwVertexAttribL4d (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL4d(index, x, y, z, w);
}

void glwVertexAttribL4dv (GLuint index, const GLdouble *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribL4dv(index, v);
}

void glwVertexAttribLFormat (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribLFormat(attribindex, size, type, relativeoffset);
}

void glwVertexAttribLPointer (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribLPointer(index, size, type, stride, pointer);
}

void glwVertexAttribP1ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP1ui(index, type, normalized, value);
}

void glwVertexAttribP1uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP1uiv(index, type, normalized, value);
}

void glwVertexAttribP2ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP2ui(index, type, normalized, value);
}

void glwVertexAttribP2uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP2uiv(index, type, normalized, value);
}

void glwVertexAttribP3ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP3ui(index, type, normalized, value);
}

void glwVertexAttribP3uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP3uiv(index, type, normalized, value);
}

void glwVertexAttribP4ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP4ui(index, type, normalized, value);
}

void glwVertexAttribP4uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribP4uiv(index, type, normalized, value);
}

void glwVertexAttribPointer (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void glwVertexBindingDivisor (GLuint bindingindex, GLuint divisor)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->vertexBindingDivisor(bindingindex, divisor);
}

void glwViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->viewport(x, y, width, height);
}

void glwViewportArrayv (GLuint first, GLsizei count, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->viewportArrayv(first, count, v);
}

void glwViewportIndexedf (GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->viewportIndexedf(index, x, y, w, h);
}

void glwViewportIndexedfv (GLuint index, const GLfloat *v)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->viewportIndexedfv(index, v);
}

void glwWaitSync (GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    const glw::Functions* gl = glw::getCurrentThreadFunctions();
    if (!gl)
        return;
    gl->waitSync(sync, flags, timeout);
}
