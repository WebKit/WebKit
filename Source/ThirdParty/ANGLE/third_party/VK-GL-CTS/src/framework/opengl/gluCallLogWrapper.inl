/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision 7d0cb181dace7461bc4f26650fec71efeacc18a5.
 */

void CallLogWrapper::glActiveShaderProgram (glw::GLuint pipeline, glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glActiveShaderProgram(" << pipeline << ", " << program << ");" << TestLog::EndMessage;
	m_gl.activeShaderProgram(pipeline, program);
}

void CallLogWrapper::glActiveTexture (glw::GLenum texture)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glActiveTexture(" << getTextureUnitStr(texture) << ");" << TestLog::EndMessage;
	m_gl.activeTexture(texture);
}

void CallLogWrapper::glAttachShader (glw::GLuint program, glw::GLuint shader)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glAttachShader(" << program << ", " << shader << ");" << TestLog::EndMessage;
	m_gl.attachShader(program, shader);
}

void CallLogWrapper::glBeginConditionalRender (glw::GLuint id, glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginConditionalRender(" << id << ", " << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.beginConditionalRender(id, mode);
}

void CallLogWrapper::glBeginQuery (glw::GLenum target, glw::GLuint id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginQuery(" << getQueryTargetStr(target) << ", " << id << ");" << TestLog::EndMessage;
	m_gl.beginQuery(target, id);
}

void CallLogWrapper::glBeginQueryIndexed (glw::GLenum target, glw::GLuint index, glw::GLuint id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginQueryIndexed(" << toHex(target) << ", " << index << ", " << id << ");" << TestLog::EndMessage;
	m_gl.beginQueryIndexed(target, index, id);
}

void CallLogWrapper::glBeginTransformFeedback (glw::GLenum primitiveMode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginTransformFeedback(" << getPrimitiveTypeStr(primitiveMode) << ");" << TestLog::EndMessage;
	m_gl.beginTransformFeedback(primitiveMode);
}

void CallLogWrapper::glBindAttribLocation (glw::GLuint program, glw::GLuint index, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindAttribLocation(" << program << ", " << index << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	m_gl.bindAttribLocation(program, index, name);
}

void CallLogWrapper::glBindBuffer (glw::GLenum target, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBuffer(" << getBufferTargetStr(target) << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.bindBuffer(target, buffer);
}

void CallLogWrapper::glBindBufferBase (glw::GLenum target, glw::GLuint index, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBufferBase(" << getBufferTargetStr(target) << ", " << index << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.bindBufferBase(target, index, buffer);
}

void CallLogWrapper::glBindBufferRange (glw::GLenum target, glw::GLuint index, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBufferRange(" << getBufferTargetStr(target) << ", " << index << ", " << buffer << ", " << offset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.bindBufferRange(target, index, buffer, offset, size);
}

void CallLogWrapper::glBindBuffersBase (glw::GLenum target, glw::GLuint first, glw::GLsizei count, const glw::GLuint *buffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBuffersBase(" << toHex(target) << ", " << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(buffers))) << ");" << TestLog::EndMessage;
	m_gl.bindBuffersBase(target, first, count, buffers);
}

void CallLogWrapper::glBindBuffersRange (glw::GLenum target, glw::GLuint first, glw::GLsizei count, const glw::GLuint *buffers, const glw::GLintptr *offsets, const glw::GLsizeiptr *sizes)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBuffersRange(" << toHex(target) << ", " << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(buffers))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(offsets))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(sizes))) << ");" << TestLog::EndMessage;
	m_gl.bindBuffersRange(target, first, count, buffers, offsets, sizes);
}

void CallLogWrapper::glBindFragDataLocation (glw::GLuint program, glw::GLuint color, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindFragDataLocation(" << program << ", " << color << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	m_gl.bindFragDataLocation(program, color, name);
}

void CallLogWrapper::glBindFragDataLocationIndexed (glw::GLuint program, glw::GLuint colorNumber, glw::GLuint index, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindFragDataLocationIndexed(" << program << ", " << colorNumber << ", " << index << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	m_gl.bindFragDataLocationIndexed(program, colorNumber, index, name);
}

void CallLogWrapper::glBindFramebuffer (glw::GLenum target, glw::GLuint framebuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindFramebuffer(" << getFramebufferTargetStr(target) << ", " << framebuffer << ");" << TestLog::EndMessage;
	m_gl.bindFramebuffer(target, framebuffer);
}

void CallLogWrapper::glBindImageTexture (glw::GLuint unit, glw::GLuint texture, glw::GLint level, glw::GLboolean layered, glw::GLint layer, glw::GLenum access, glw::GLenum format)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindImageTexture(" << unit << ", " << texture << ", " << level << ", " << getBooleanStr(layered) << ", " << layer << ", " << getImageAccessStr(access) << ", " << getUncompressedTextureFormatStr(format) << ");" << TestLog::EndMessage;
	m_gl.bindImageTexture(unit, texture, level, layered, layer, access, format);
}

void CallLogWrapper::glBindImageTextures (glw::GLuint first, glw::GLsizei count, const glw::GLuint *textures)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindImageTextures(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(textures))) << ");" << TestLog::EndMessage;
	m_gl.bindImageTextures(first, count, textures);
}

void CallLogWrapper::glBindMultiTextureEXT (glw::GLenum texunit, glw::GLenum target, glw::GLuint texture)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindMultiTextureEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << texture << ");" << TestLog::EndMessage;
	m_gl.bindMultiTextureEXT(texunit, target, texture);
}

void CallLogWrapper::glBindProgramPipeline (glw::GLuint pipeline)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindProgramPipeline(" << pipeline << ");" << TestLog::EndMessage;
	m_gl.bindProgramPipeline(pipeline);
}

void CallLogWrapper::glBindRenderbuffer (glw::GLenum target, glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindRenderbuffer(" << getFramebufferTargetStr(target) << ", " << renderbuffer << ");" << TestLog::EndMessage;
	m_gl.bindRenderbuffer(target, renderbuffer);
}

void CallLogWrapper::glBindSampler (glw::GLuint unit, glw::GLuint sampler)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindSampler(" << unit << ", " << sampler << ");" << TestLog::EndMessage;
	m_gl.bindSampler(unit, sampler);
}

void CallLogWrapper::glBindSamplers (glw::GLuint first, glw::GLsizei count, const glw::GLuint *samplers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindSamplers(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(samplers))) << ");" << TestLog::EndMessage;
	m_gl.bindSamplers(first, count, samplers);
}

void CallLogWrapper::glBindTexture (glw::GLenum target, glw::GLuint texture)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTexture(" << getTextureTargetStr(target) << ", " << texture << ");" << TestLog::EndMessage;
	m_gl.bindTexture(target, texture);
}

void CallLogWrapper::glBindTextureUnit (glw::GLuint unit, glw::GLuint texture)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTextureUnit(" << unit << ", " << texture << ");" << TestLog::EndMessage;
	m_gl.bindTextureUnit(unit, texture);
}

void CallLogWrapper::glBindTextures (glw::GLuint first, glw::GLsizei count, const glw::GLuint *textures)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTextures(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(textures))) << ");" << TestLog::EndMessage;
	m_gl.bindTextures(first, count, textures);
}

void CallLogWrapper::glBindTransformFeedback (glw::GLenum target, glw::GLuint id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTransformFeedback(" << getTransformFeedbackTargetStr(target) << ", " << id << ");" << TestLog::EndMessage;
	m_gl.bindTransformFeedback(target, id);
}

void CallLogWrapper::glBindVertexArray (glw::GLuint array)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindVertexArray(" << array << ");" << TestLog::EndMessage;
	m_gl.bindVertexArray(array);
}

void CallLogWrapper::glBindVertexBuffer (glw::GLuint bindingindex, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindVertexBuffer(" << bindingindex << ", " << buffer << ", " << offset << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.bindVertexBuffer(bindingindex, buffer, offset, stride);
}

void CallLogWrapper::glBindVertexBuffers (glw::GLuint first, glw::GLsizei count, const glw::GLuint *buffers, const glw::GLintptr *offsets, const glw::GLsizei *strides)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindVertexBuffers(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(buffers))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(offsets))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(strides))) << ");" << TestLog::EndMessage;
	m_gl.bindVertexBuffers(first, count, buffers, offsets, strides);
}

void CallLogWrapper::glBlendBarrier (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendBarrier(" << ");" << TestLog::EndMessage;
	m_gl.blendBarrier();
}

void CallLogWrapper::glBlendColor (glw::GLfloat red, glw::GLfloat green, glw::GLfloat blue, glw::GLfloat alpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendColor(" << red << ", " << green << ", " << blue << ", " << alpha << ");" << TestLog::EndMessage;
	m_gl.blendColor(red, green, blue, alpha);
}

void CallLogWrapper::glBlendEquation (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquation(" << getBlendEquationStr(mode) << ");" << TestLog::EndMessage;
	m_gl.blendEquation(mode);
}

void CallLogWrapper::glBlendEquationSeparate (glw::GLenum modeRGB, glw::GLenum modeAlpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquationSeparate(" << getBlendEquationStr(modeRGB) << ", " << getBlendEquationStr(modeAlpha) << ");" << TestLog::EndMessage;
	m_gl.blendEquationSeparate(modeRGB, modeAlpha);
}

void CallLogWrapper::glBlendEquationSeparatei (glw::GLuint buf, glw::GLenum modeRGB, glw::GLenum modeAlpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquationSeparatei(" << buf << ", " << getBlendEquationStr(modeRGB) << ", " << getBlendEquationStr(modeAlpha) << ");" << TestLog::EndMessage;
	m_gl.blendEquationSeparatei(buf, modeRGB, modeAlpha);
}

void CallLogWrapper::glBlendEquationi (glw::GLuint buf, glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquationi(" << buf << ", " << getBlendEquationStr(mode) << ");" << TestLog::EndMessage;
	m_gl.blendEquationi(buf, mode);
}

void CallLogWrapper::glBlendFunc (glw::GLenum sfactor, glw::GLenum dfactor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFunc(" << getBlendFactorStr(sfactor) << ", " << getBlendFactorStr(dfactor) << ");" << TestLog::EndMessage;
	m_gl.blendFunc(sfactor, dfactor);
}

void CallLogWrapper::glBlendFuncSeparate (glw::GLenum sfactorRGB, glw::GLenum dfactorRGB, glw::GLenum sfactorAlpha, glw::GLenum dfactorAlpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFuncSeparate(" << getBlendFactorStr(sfactorRGB) << ", " << getBlendFactorStr(dfactorRGB) << ", " << getBlendFactorStr(sfactorAlpha) << ", " << getBlendFactorStr(dfactorAlpha) << ");" << TestLog::EndMessage;
	m_gl.blendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

void CallLogWrapper::glBlendFuncSeparatei (glw::GLuint buf, glw::GLenum srcRGB, glw::GLenum dstRGB, glw::GLenum srcAlpha, glw::GLenum dstAlpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFuncSeparatei(" << buf << ", " << toHex(srcRGB) << ", " << toHex(dstRGB) << ", " << toHex(srcAlpha) << ", " << toHex(dstAlpha) << ");" << TestLog::EndMessage;
	m_gl.blendFuncSeparatei(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void CallLogWrapper::glBlendFunci (glw::GLuint buf, glw::GLenum src, glw::GLenum dst)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFunci(" << buf << ", " << toHex(src) << ", " << toHex(dst) << ");" << TestLog::EndMessage;
	m_gl.blendFunci(buf, src, dst);
}

void CallLogWrapper::glBlitFramebuffer (glw::GLint srcX0, glw::GLint srcY0, glw::GLint srcX1, glw::GLint srcY1, glw::GLint dstX0, glw::GLint dstY0, glw::GLint dstX1, glw::GLint dstY1, glw::GLbitfield mask, glw::GLenum filter)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlitFramebuffer(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", " << dstY1 << ", " << getBufferMaskStr(mask) << ", " << getTextureFilterStr(filter) << ");" << TestLog::EndMessage;
	m_gl.blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void CallLogWrapper::glBlitNamedFramebuffer (glw::GLuint readFramebuffer, glw::GLuint drawFramebuffer, glw::GLint srcX0, glw::GLint srcY0, glw::GLint srcX1, glw::GLint srcY1, glw::GLint dstX0, glw::GLint dstY0, glw::GLint dstX1, glw::GLint dstY1, glw::GLbitfield mask, glw::GLenum filter)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlitNamedFramebuffer(" << readFramebuffer << ", " << drawFramebuffer << ", " << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", " << dstY1 << ", " << toHex(mask) << ", " << toHex(filter) << ");" << TestLog::EndMessage;
	m_gl.blitNamedFramebuffer(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void CallLogWrapper::glBufferData (glw::GLenum target, glw::GLsizeiptr size, const void *data, glw::GLenum usage)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferData(" << getBufferTargetStr(target) << ", " << size << ", " << data << ", " << getUsageStr(usage) << ");" << TestLog::EndMessage;
	m_gl.bufferData(target, size, data, usage);
}

void CallLogWrapper::glBufferPageCommitmentARB (glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr size, glw::GLboolean commit)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferPageCommitmentARB(" << toHex(target) << ", " << offset << ", " << size << ", " << getBooleanStr(commit) << ");" << TestLog::EndMessage;
	m_gl.bufferPageCommitmentARB(target, offset, size, commit);
}

void CallLogWrapper::glBufferStorage (glw::GLenum target, glw::GLsizeiptr size, const void *data, glw::GLbitfield flags)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferStorage(" << toHex(target) << ", " << size << ", " << data << ", " << toHex(flags) << ");" << TestLog::EndMessage;
	m_gl.bufferStorage(target, size, data, flags);
}

void CallLogWrapper::glBufferSubData (glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr size, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferSubData(" << getBufferTargetStr(target) << ", " << offset << ", " << size << ", " << data << ");" << TestLog::EndMessage;
	m_gl.bufferSubData(target, offset, size, data);
}

glw::GLenum CallLogWrapper::glCheckFramebufferStatus (glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCheckFramebufferStatus(" << getFramebufferTargetStr(target) << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.checkFramebufferStatus(target);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getFramebufferStatusStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLenum CallLogWrapper::glCheckNamedFramebufferStatus (glw::GLuint framebuffer, glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCheckNamedFramebufferStatus(" << framebuffer << ", " << toHex(target) << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.checkNamedFramebufferStatus(framebuffer, target);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLenum CallLogWrapper::glCheckNamedFramebufferStatusEXT (glw::GLuint framebuffer, glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCheckNamedFramebufferStatusEXT(" << framebuffer << ", " << toHex(target) << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.checkNamedFramebufferStatusEXT(framebuffer, target);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glClampColor (glw::GLenum target, glw::GLenum clamp)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClampColor(" << toHex(target) << ", " << toHex(clamp) << ");" << TestLog::EndMessage;
	m_gl.clampColor(target, clamp);
}

void CallLogWrapper::glClear (glw::GLbitfield mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClear(" << getBufferMaskStr(mask) << ");" << TestLog::EndMessage;
	m_gl.clear(mask);
}

void CallLogWrapper::glClearBufferData (glw::GLenum target, glw::GLenum internalformat, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferData(" << toHex(target) << ", " << toHex(internalformat) << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearBufferData(target, internalformat, format, type, data);
}

void CallLogWrapper::glClearBufferSubData (glw::GLenum target, glw::GLenum internalformat, glw::GLintptr offset, glw::GLsizeiptr size, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferSubData(" << toHex(target) << ", " << toHex(internalformat) << ", " << offset << ", " << size << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearBufferSubData(target, internalformat, offset, size, format, type, data);
}

void CallLogWrapper::glClearBufferfi (glw::GLenum buffer, glw::GLint drawbuffer, glw::GLfloat depth, glw::GLint stencil)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferfi(" << getBufferStr(buffer) << ", " << drawbuffer << ", " << depth << ", " << stencil << ");" << TestLog::EndMessage;
	m_gl.clearBufferfi(buffer, drawbuffer, depth, stencil);
}

void CallLogWrapper::glClearBufferfv (glw::GLenum buffer, glw::GLint drawbuffer, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferfv(" << getBufferStr(buffer) << ", " << drawbuffer << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.clearBufferfv(buffer, drawbuffer, value);
}

void CallLogWrapper::glClearBufferiv (glw::GLenum buffer, glw::GLint drawbuffer, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferiv(" << getBufferStr(buffer) << ", " << drawbuffer << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.clearBufferiv(buffer, drawbuffer, value);
}

void CallLogWrapper::glClearBufferuiv (glw::GLenum buffer, glw::GLint drawbuffer, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferuiv(" << getBufferStr(buffer) << ", " << drawbuffer << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.clearBufferuiv(buffer, drawbuffer, value);
}

void CallLogWrapper::glClearColor (glw::GLfloat red, glw::GLfloat green, glw::GLfloat blue, glw::GLfloat alpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearColor(" << red << ", " << green << ", " << blue << ", " << alpha << ");" << TestLog::EndMessage;
	m_gl.clearColor(red, green, blue, alpha);
}

void CallLogWrapper::glClearDepth (glw::GLdouble depth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearDepth(" << depth << ");" << TestLog::EndMessage;
	m_gl.clearDepth(depth);
}

void CallLogWrapper::glClearDepthf (glw::GLfloat d)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearDepthf(" << d << ");" << TestLog::EndMessage;
	m_gl.clearDepthf(d);
}

void CallLogWrapper::glClearNamedBufferData (glw::GLuint buffer, glw::GLenum internalformat, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedBufferData(" << buffer << ", " << toHex(internalformat) << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearNamedBufferData(buffer, internalformat, format, type, data);
}

void CallLogWrapper::glClearNamedBufferDataEXT (glw::GLuint buffer, glw::GLenum internalformat, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedBufferDataEXT(" << buffer << ", " << toHex(internalformat) << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearNamedBufferDataEXT(buffer, internalformat, format, type, data);
}

void CallLogWrapper::glClearNamedBufferSubData (glw::GLuint buffer, glw::GLenum internalformat, glw::GLintptr offset, glw::GLsizeiptr size, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedBufferSubData(" << buffer << ", " << toHex(internalformat) << ", " << offset << ", " << size << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearNamedBufferSubData(buffer, internalformat, offset, size, format, type, data);
}

void CallLogWrapper::glClearNamedBufferSubDataEXT (glw::GLuint buffer, glw::GLenum internalformat, glw::GLsizeiptr offset, glw::GLsizeiptr size, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedBufferSubDataEXT(" << buffer << ", " << toHex(internalformat) << ", " << offset << ", " << size << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);
}

void CallLogWrapper::glClearNamedFramebufferfi (glw::GLuint framebuffer, glw::GLenum buffer, glw::GLint drawbuffer, glw::GLfloat depth, glw::GLint stencil)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedFramebufferfi(" << framebuffer << ", " << toHex(buffer) << ", " << drawbuffer << ", " << depth << ", " << stencil << ");" << TestLog::EndMessage;
	m_gl.clearNamedFramebufferfi(framebuffer, buffer, drawbuffer, depth, stencil);
}

void CallLogWrapper::glClearNamedFramebufferfv (glw::GLuint framebuffer, glw::GLenum buffer, glw::GLint drawbuffer, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedFramebufferfv(" << framebuffer << ", " << toHex(buffer) << ", " << drawbuffer << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.clearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);
}

void CallLogWrapper::glClearNamedFramebufferiv (glw::GLuint framebuffer, glw::GLenum buffer, glw::GLint drawbuffer, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedFramebufferiv(" << framebuffer << ", " << toHex(buffer) << ", " << drawbuffer << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.clearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value);
}

void CallLogWrapper::glClearNamedFramebufferuiv (glw::GLuint framebuffer, glw::GLenum buffer, glw::GLint drawbuffer, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearNamedFramebufferuiv(" << framebuffer << ", " << toHex(buffer) << ", " << drawbuffer << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.clearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value);
}

void CallLogWrapper::glClearStencil (glw::GLint s)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearStencil(" << s << ");" << TestLog::EndMessage;
	m_gl.clearStencil(s);
}

void CallLogWrapper::glClearTexImage (glw::GLuint texture, glw::GLint level, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearTexImage(" << texture << ", " << level << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearTexImage(texture, level, format, type, data);
}

void CallLogWrapper::glClearTexSubImage (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearTexSubImage(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << toHex(type) << ", " << data << ");" << TestLog::EndMessage;
	m_gl.clearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
}

void CallLogWrapper::glClientAttribDefaultEXT (glw::GLbitfield mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClientAttribDefaultEXT(" << toHex(mask) << ");" << TestLog::EndMessage;
	m_gl.clientAttribDefaultEXT(mask);
}

glw::GLenum CallLogWrapper::glClientWaitSync (glw::GLsync sync, glw::GLbitfield flags, glw::GLuint64 timeout)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClientWaitSync(" << sync << ", " << toHex(flags) << ", " << timeout << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.clientWaitSync(sync, flags, timeout);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glClipControl (glw::GLenum origin, glw::GLenum depth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClipControl(" << toHex(origin) << ", " << toHex(depth) << ");" << TestLog::EndMessage;
	m_gl.clipControl(origin, depth);
}

void CallLogWrapper::glColorMask (glw::GLboolean red, glw::GLboolean green, glw::GLboolean blue, glw::GLboolean alpha)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glColorMask(" << getBooleanStr(red) << ", " << getBooleanStr(green) << ", " << getBooleanStr(blue) << ", " << getBooleanStr(alpha) << ");" << TestLog::EndMessage;
	m_gl.colorMask(red, green, blue, alpha);
}

void CallLogWrapper::glColorMaski (glw::GLuint index, glw::GLboolean r, glw::GLboolean g, glw::GLboolean b, glw::GLboolean a)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glColorMaski(" << index << ", " << getBooleanStr(r) << ", " << getBooleanStr(g) << ", " << getBooleanStr(b) << ", " << getBooleanStr(a) << ");" << TestLog::EndMessage;
	m_gl.colorMaski(index, r, g, b, a);
}

void CallLogWrapper::glCompileShader (glw::GLuint shader)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompileShader(" << shader << ");" << TestLog::EndMessage;
	m_gl.compileShader(shader);
}

void CallLogWrapper::glCompressedMultiTexImage1DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLint border, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedMultiTexImage1DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << border << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedMultiTexImage1DEXT(texunit, target, level, internalformat, width, border, imageSize, bits);
}

void CallLogWrapper::glCompressedMultiTexImage2DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLint border, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedMultiTexImage2DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << border << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedMultiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, imageSize, bits);
}

void CallLogWrapper::glCompressedMultiTexImage3DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedMultiTexImage3DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedMultiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void CallLogWrapper::glCompressedMultiTexSubImage1DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedMultiTexSubImage1DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, imageSize, bits);
}

void CallLogWrapper::glCompressedMultiTexSubImage2DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedMultiTexSubImage2DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << toHex(format) << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void CallLogWrapper::glCompressedMultiTexSubImage3DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedMultiTexSubImage3DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void CallLogWrapper::glCompressedTexImage1D (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLint border, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage1D(" << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << border << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage1D(target, level, internalformat, width, border, imageSize, data);
}

void CallLogWrapper::glCompressedTexImage2D (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLint border, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage2D(" << getTextureTargetStr(target) << ", " << level << ", " << getCompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << border << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void CallLogWrapper::glCompressedTexImage3D (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage3D(" << getTextureTargetStr(target) << ", " << level << ", " << getCompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void CallLogWrapper::glCompressedTexImage3DOES (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage3DOES(" << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage3DOES(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void CallLogWrapper::glCompressedTexSubImage1D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage1D(" << toHex(target) << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
}

void CallLogWrapper::glCompressedTexSubImage2D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage2D(" << getTextureTargetStr(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << getCompressedTextureFormatStr(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void CallLogWrapper::glCompressedTexSubImage3D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage3D(" << getTextureTargetStr(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << getCompressedTextureFormatStr(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void CallLogWrapper::glCompressedTexSubImage3DOES (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage3DOES(" << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void CallLogWrapper::glCompressedTextureImage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLint border, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureImage1DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << border << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedTextureImage1DEXT(texture, target, level, internalformat, width, border, imageSize, bits);
}

void CallLogWrapper::glCompressedTextureImage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLint border, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureImage2DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << border << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedTextureImage2DEXT(texture, target, level, internalformat, width, height, border, imageSize, bits);
}

void CallLogWrapper::glCompressedTextureImage3DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureImage3DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void CallLogWrapper::glCompressedTextureSubImage1D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureSubImage1D(" << texture << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTextureSubImage1D(texture, level, xoffset, width, format, imageSize, data);
}

void CallLogWrapper::glCompressedTextureSubImage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureSubImage1DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedTextureSubImage1DEXT(texture, target, level, xoffset, width, format, imageSize, bits);
}

void CallLogWrapper::glCompressedTextureSubImage2D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureSubImage2D(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << toHex(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void CallLogWrapper::glCompressedTextureSubImage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureSubImage2DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << toHex(format) << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void CallLogWrapper::glCompressedTextureSubImage3D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLsizei imageSize, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureSubImage3D(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << imageSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.compressedTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void CallLogWrapper::glCompressedTextureSubImage3DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLsizei imageSize, const void *bits)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTextureSubImage3DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << imageSize << ", " << bits << ");" << TestLog::EndMessage;
	m_gl.compressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void CallLogWrapper::glCopyBufferSubData (glw::GLenum readTarget, glw::GLenum writeTarget, glw::GLintptr readOffset, glw::GLintptr writeOffset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyBufferSubData(" << toHex(readTarget) << ", " << toHex(writeTarget) << ", " << readOffset << ", " << writeOffset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.copyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void CallLogWrapper::glCopyImageSubData (glw::GLuint srcName, glw::GLenum srcTarget, glw::GLint srcLevel, glw::GLint srcX, glw::GLint srcY, glw::GLint srcZ, glw::GLuint dstName, glw::GLenum dstTarget, glw::GLint dstLevel, glw::GLint dstX, glw::GLint dstY, glw::GLint dstZ, glw::GLsizei srcWidth, glw::GLsizei srcHeight, glw::GLsizei srcDepth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyImageSubData(" << srcName << ", " << toHex(srcTarget) << ", " << srcLevel << ", " << srcX << ", " << srcY << ", " << srcZ << ", " << dstName << ", " << toHex(dstTarget) << ", " << dstLevel << ", " << dstX << ", " << dstY << ", " << dstZ << ", " << srcWidth << ", " << srcHeight << ", " << srcDepth << ");" << TestLog::EndMessage;
	m_gl.copyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void CallLogWrapper::glCopyMultiTexImage1DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLint border)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyMultiTexImage1DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << x << ", " << y << ", " << width << ", " << border << ");" << TestLog::EndMessage;
	m_gl.copyMultiTexImage1DEXT(texunit, target, level, internalformat, x, y, width, border);
}

void CallLogWrapper::glCopyMultiTexImage2DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height, glw::GLint border)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyMultiTexImage2DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << x << ", " << y << ", " << width << ", " << height << ", " << border << ");" << TestLog::EndMessage;
	m_gl.copyMultiTexImage2DEXT(texunit, target, level, internalformat, x, y, width, height, border);
}

void CallLogWrapper::glCopyMultiTexSubImage1DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint x, glw::GLint y, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyMultiTexSubImage1DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << x << ", " << y << ", " << width << ");" << TestLog::EndMessage;
	m_gl.copyMultiTexSubImage1DEXT(texunit, target, level, xoffset, x, y, width);
}

void CallLogWrapper::glCopyMultiTexSubImage2DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyMultiTexSubImage2DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, x, y, width, height);
}

void CallLogWrapper::glCopyMultiTexSubImage3DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyMultiTexSubImage3DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void CallLogWrapper::glCopyNamedBufferSubData (glw::GLuint readBuffer, glw::GLuint writeBuffer, glw::GLintptr readOffset, glw::GLintptr writeOffset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyNamedBufferSubData(" << readBuffer << ", " << writeBuffer << ", " << readOffset << ", " << writeOffset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.copyNamedBufferSubData(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void CallLogWrapper::glCopyTexImage1D (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLint border)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexImage1D(" << getTextureTargetStr(target) << ", " << level << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << x << ", " << y << ", " << width << ", " << border << ");" << TestLog::EndMessage;
	m_gl.copyTexImage1D(target, level, internalformat, x, y, width, border);
}

void CallLogWrapper::glCopyTexImage2D (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height, glw::GLint border)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexImage2D(" << getTextureTargetStr(target) << ", " << level << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << x << ", " << y << ", " << width << ", " << height << ", " << border << ");" << TestLog::EndMessage;
	m_gl.copyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void CallLogWrapper::glCopyTexSubImage1D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint x, glw::GLint y, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage1D(" << toHex(target) << ", " << level << ", " << xoffset << ", " << x << ", " << y << ", " << width << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage1D(target, level, xoffset, x, y, width);
}

void CallLogWrapper::glCopyTexSubImage2D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage2D(" << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void CallLogWrapper::glCopyTexSubImage3D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage3D(" << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void CallLogWrapper::glCopyTexSubImage3DOES (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage3DOES(" << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void CallLogWrapper::glCopyTextureImage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLint border)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureImage1DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << x << ", " << y << ", " << width << ", " << border << ");" << TestLog::EndMessage;
	m_gl.copyTextureImage1DEXT(texture, target, level, internalformat, x, y, width, border);
}

void CallLogWrapper::glCopyTextureImage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height, glw::GLint border)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureImage2DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << x << ", " << y << ", " << width << ", " << height << ", " << border << ");" << TestLog::EndMessage;
	m_gl.copyTextureImage2DEXT(texture, target, level, internalformat, x, y, width, height, border);
}

void CallLogWrapper::glCopyTextureSubImage1D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint x, glw::GLint y, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureSubImage1D(" << texture << ", " << level << ", " << xoffset << ", " << x << ", " << y << ", " << width << ");" << TestLog::EndMessage;
	m_gl.copyTextureSubImage1D(texture, level, xoffset, x, y, width);
}

void CallLogWrapper::glCopyTextureSubImage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint x, glw::GLint y, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureSubImage1DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << x << ", " << y << ", " << width << ");" << TestLog::EndMessage;
	m_gl.copyTextureSubImage1DEXT(texture, target, level, xoffset, x, y, width);
}

void CallLogWrapper::glCopyTextureSubImage2D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureSubImage2D(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTextureSubImage2D(texture, level, xoffset, yoffset, x, y, width, height);
}

void CallLogWrapper::glCopyTextureSubImage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureSubImage2DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, x, y, width, height);
}

void CallLogWrapper::glCopyTextureSubImage3D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureSubImage3D(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void CallLogWrapper::glCopyTextureSubImage3DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTextureSubImage3DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.copyTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void CallLogWrapper::glCreateBuffers (glw::GLsizei n, glw::GLuint *buffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateBuffers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(buffers))) << ");" << TestLog::EndMessage;
	m_gl.createBuffers(n, buffers);
}

void CallLogWrapper::glCreateFramebuffers (glw::GLsizei n, glw::GLuint *framebuffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateFramebuffers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(framebuffers))) << ");" << TestLog::EndMessage;
	m_gl.createFramebuffers(n, framebuffers);
}

glw::GLuint CallLogWrapper::glCreateProgram (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateProgram(" << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.createProgram();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glCreateProgramPipelines (glw::GLsizei n, glw::GLuint *pipelines)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateProgramPipelines(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(pipelines))) << ");" << TestLog::EndMessage;
	m_gl.createProgramPipelines(n, pipelines);
}

void CallLogWrapper::glCreateQueries (glw::GLenum target, glw::GLsizei n, glw::GLuint *ids)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateQueries(" << toHex(target) << ", " << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(ids))) << ");" << TestLog::EndMessage;
	m_gl.createQueries(target, n, ids);
}

void CallLogWrapper::glCreateRenderbuffers (glw::GLsizei n, glw::GLuint *renderbuffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateRenderbuffers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(renderbuffers))) << ");" << TestLog::EndMessage;
	m_gl.createRenderbuffers(n, renderbuffers);
}

void CallLogWrapper::glCreateSamplers (glw::GLsizei n, glw::GLuint *samplers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateSamplers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(samplers))) << ");" << TestLog::EndMessage;
	m_gl.createSamplers(n, samplers);
}

glw::GLuint CallLogWrapper::glCreateShader (glw::GLenum type)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateShader(" << getShaderTypeStr(type) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.createShader(type);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLuint CallLogWrapper::glCreateShaderProgramv (glw::GLenum type, glw::GLsizei count, const glw::GLchar *const*strings)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateShaderProgramv(" << toHex(type) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(strings))) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.createShaderProgramv(type, count, strings);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glCreateTextures (glw::GLenum target, glw::GLsizei n, glw::GLuint *textures)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateTextures(" << toHex(target) << ", " << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(textures))) << ");" << TestLog::EndMessage;
	m_gl.createTextures(target, n, textures);
}

void CallLogWrapper::glCreateTransformFeedbacks (glw::GLsizei n, glw::GLuint *ids)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateTransformFeedbacks(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(ids))) << ");" << TestLog::EndMessage;
	m_gl.createTransformFeedbacks(n, ids);
}

void CallLogWrapper::glCreateVertexArrays (glw::GLsizei n, glw::GLuint *arrays)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateVertexArrays(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(arrays))) << ");" << TestLog::EndMessage;
	m_gl.createVertexArrays(n, arrays);
}

void CallLogWrapper::glCullFace (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCullFace(" << getFaceStr(mode) << ");" << TestLog::EndMessage;
	m_gl.cullFace(mode);
}

void CallLogWrapper::glDebugMessageCallback (glw::GLDEBUGPROC callback, const void *userParam)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDebugMessageCallback(" << toHex(reinterpret_cast<uintptr_t>(callback)) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(userParam))) << ");" << TestLog::EndMessage;
	m_gl.debugMessageCallback(callback, userParam);
}

void CallLogWrapper::glDebugMessageControl (glw::GLenum source, glw::GLenum type, glw::GLenum severity, glw::GLsizei count, const glw::GLuint *ids, glw::GLboolean enabled)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDebugMessageControl(" << getDebugMessageSourceStr(source) << ", " << getDebugMessageTypeStr(type) << ", " << getDebugMessageSeverityStr(severity) << ", " << count << ", " << getPointerStr(ids, (count)) << ", " << getBooleanStr(enabled) << ");" << TestLog::EndMessage;
	m_gl.debugMessageControl(source, type, severity, count, ids, enabled);
}

void CallLogWrapper::glDebugMessageInsert (glw::GLenum source, glw::GLenum type, glw::GLuint id, glw::GLenum severity, glw::GLsizei length, const glw::GLchar *buf)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDebugMessageInsert(" << getDebugMessageSourceStr(source) << ", " << getDebugMessageTypeStr(type) << ", " << id << ", " << getDebugMessageSeverityStr(severity) << ", " << length << ", " << getStringStr(buf) << ");" << TestLog::EndMessage;
	m_gl.debugMessageInsert(source, type, id, severity, length, buf);
}

void CallLogWrapper::glDeleteBuffers (glw::GLsizei n, const glw::GLuint *buffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteBuffers(" << n << ", " << getPointerStr(buffers, n) << ");" << TestLog::EndMessage;
	m_gl.deleteBuffers(n, buffers);
}

void CallLogWrapper::glDeleteFramebuffers (glw::GLsizei n, const glw::GLuint *framebuffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteFramebuffers(" << n << ", " << getPointerStr(framebuffers, n) << ");" << TestLog::EndMessage;
	m_gl.deleteFramebuffers(n, framebuffers);
}

void CallLogWrapper::glDeleteProgram (glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteProgram(" << program << ");" << TestLog::EndMessage;
	m_gl.deleteProgram(program);
}

void CallLogWrapper::glDeleteProgramPipelines (glw::GLsizei n, const glw::GLuint *pipelines)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteProgramPipelines(" << n << ", " << getPointerStr(pipelines, n) << ");" << TestLog::EndMessage;
	m_gl.deleteProgramPipelines(n, pipelines);
}

void CallLogWrapper::glDeleteQueries (glw::GLsizei n, const glw::GLuint *ids)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteQueries(" << n << ", " << getPointerStr(ids, n) << ");" << TestLog::EndMessage;
	m_gl.deleteQueries(n, ids);
}

void CallLogWrapper::glDeleteRenderbuffers (glw::GLsizei n, const glw::GLuint *renderbuffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteRenderbuffers(" << n << ", " << getPointerStr(renderbuffers, n) << ");" << TestLog::EndMessage;
	m_gl.deleteRenderbuffers(n, renderbuffers);
}

void CallLogWrapper::glDeleteSamplers (glw::GLsizei count, const glw::GLuint *samplers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteSamplers(" << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(samplers))) << ");" << TestLog::EndMessage;
	m_gl.deleteSamplers(count, samplers);
}

void CallLogWrapper::glDeleteShader (glw::GLuint shader)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteShader(" << shader << ");" << TestLog::EndMessage;
	m_gl.deleteShader(shader);
}

void CallLogWrapper::glDeleteSync (glw::GLsync sync)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteSync(" << sync << ");" << TestLog::EndMessage;
	m_gl.deleteSync(sync);
}

void CallLogWrapper::glDeleteTextures (glw::GLsizei n, const glw::GLuint *textures)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteTextures(" << n << ", " << getPointerStr(textures, n) << ");" << TestLog::EndMessage;
	m_gl.deleteTextures(n, textures);
}

void CallLogWrapper::glDeleteTransformFeedbacks (glw::GLsizei n, const glw::GLuint *ids)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteTransformFeedbacks(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(ids))) << ");" << TestLog::EndMessage;
	m_gl.deleteTransformFeedbacks(n, ids);
}

void CallLogWrapper::glDeleteVertexArrays (glw::GLsizei n, const glw::GLuint *arrays)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteVertexArrays(" << n << ", " << getPointerStr(arrays, n) << ");" << TestLog::EndMessage;
	m_gl.deleteVertexArrays(n, arrays);
}

void CallLogWrapper::glDepthBoundsEXT (glw::GLclampd zmin, glw::GLclampd zmax)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthBoundsEXT(" << zmin << ", " << zmax << ");" << TestLog::EndMessage;
	m_gl.depthBoundsEXT(zmin, zmax);
}

void CallLogWrapper::glDepthFunc (glw::GLenum func)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthFunc(" << getCompareFuncStr(func) << ");" << TestLog::EndMessage;
	m_gl.depthFunc(func);
}

void CallLogWrapper::glDepthMask (glw::GLboolean flag)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthMask(" << getBooleanStr(flag) << ");" << TestLog::EndMessage;
	m_gl.depthMask(flag);
}

void CallLogWrapper::glDepthRange (glw::GLdouble n, glw::GLdouble f)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRange(" << n << ", " << f << ");" << TestLog::EndMessage;
	m_gl.depthRange(n, f);
}

void CallLogWrapper::glDepthRangeArrayfvOES (glw::GLuint first, glw::GLsizei count, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangeArrayfvOES(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.depthRangeArrayfvOES(first, count, v);
}

void CallLogWrapper::glDepthRangeArrayv (glw::GLuint first, glw::GLsizei count, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangeArrayv(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.depthRangeArrayv(first, count, v);
}

void CallLogWrapper::glDepthRangeIndexed (glw::GLuint index, glw::GLdouble n, glw::GLdouble f)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangeIndexed(" << index << ", " << n << ", " << f << ");" << TestLog::EndMessage;
	m_gl.depthRangeIndexed(index, n, f);
}

void CallLogWrapper::glDepthRangeIndexedfOES (glw::GLuint index, glw::GLfloat n, glw::GLfloat f)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangeIndexedfOES(" << index << ", " << n << ", " << f << ");" << TestLog::EndMessage;
	m_gl.depthRangeIndexedfOES(index, n, f);
}

void CallLogWrapper::glDepthRangef (glw::GLfloat n, glw::GLfloat f)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangef(" << n << ", " << f << ");" << TestLog::EndMessage;
	m_gl.depthRangef(n, f);
}

void CallLogWrapper::glDetachShader (glw::GLuint program, glw::GLuint shader)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDetachShader(" << program << ", " << shader << ");" << TestLog::EndMessage;
	m_gl.detachShader(program, shader);
}

void CallLogWrapper::glDisable (glw::GLenum cap)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisable(" << getEnableCapStr(cap) << ");" << TestLog::EndMessage;
	m_gl.disable(cap);
}

void CallLogWrapper::glDisableClientStateIndexedEXT (glw::GLenum array, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableClientStateIndexedEXT(" << toHex(array) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.disableClientStateIndexedEXT(array, index);
}

void CallLogWrapper::glDisableClientStateiEXT (glw::GLenum array, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableClientStateiEXT(" << toHex(array) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.disableClientStateiEXT(array, index);
}

void CallLogWrapper::glDisableVertexArrayAttrib (glw::GLuint vaobj, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableVertexArrayAttrib(" << vaobj << ", " << index << ");" << TestLog::EndMessage;
	m_gl.disableVertexArrayAttrib(vaobj, index);
}

void CallLogWrapper::glDisableVertexArrayAttribEXT (glw::GLuint vaobj, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableVertexArrayAttribEXT(" << vaobj << ", " << index << ");" << TestLog::EndMessage;
	m_gl.disableVertexArrayAttribEXT(vaobj, index);
}

void CallLogWrapper::glDisableVertexArrayEXT (glw::GLuint vaobj, glw::GLenum array)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableVertexArrayEXT(" << vaobj << ", " << toHex(array) << ");" << TestLog::EndMessage;
	m_gl.disableVertexArrayEXT(vaobj, array);
}

void CallLogWrapper::glDisableVertexAttribArray (glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableVertexAttribArray(" << index << ");" << TestLog::EndMessage;
	m_gl.disableVertexAttribArray(index);
}

void CallLogWrapper::glDisablei (glw::GLenum target, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisablei(" << getIndexedEnableCapStr(target) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.disablei(target, index);
}

void CallLogWrapper::glDispatchCompute (glw::GLuint num_groups_x, glw::GLuint num_groups_y, glw::GLuint num_groups_z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDispatchCompute(" << num_groups_x << ", " << num_groups_y << ", " << num_groups_z << ");" << TestLog::EndMessage;
	m_gl.dispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

void CallLogWrapper::glDispatchComputeIndirect (glw::GLintptr indirect)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDispatchComputeIndirect(" << indirect << ");" << TestLog::EndMessage;
	m_gl.dispatchComputeIndirect(indirect);
}

void CallLogWrapper::glDrawArrays (glw::GLenum mode, glw::GLint first, glw::GLsizei count)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArrays(" << getPrimitiveTypeStr(mode) << ", " << first << ", " << count << ");" << TestLog::EndMessage;
	m_gl.drawArrays(mode, first, count);
}

void CallLogWrapper::glDrawArraysIndirect (glw::GLenum mode, const void *indirect)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArraysIndirect(" << getPrimitiveTypeStr(mode) << ", " << indirect << ");" << TestLog::EndMessage;
	m_gl.drawArraysIndirect(mode, indirect);
}

void CallLogWrapper::glDrawArraysInstanced (glw::GLenum mode, glw::GLint first, glw::GLsizei count, glw::GLsizei instancecount)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArraysInstanced(" << getPrimitiveTypeStr(mode) << ", " << first << ", " << count << ", " << instancecount << ");" << TestLog::EndMessage;
	m_gl.drawArraysInstanced(mode, first, count, instancecount);
}

void CallLogWrapper::glDrawArraysInstancedBaseInstance (glw::GLenum mode, glw::GLint first, glw::GLsizei count, glw::GLsizei instancecount, glw::GLuint baseinstance)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArraysInstancedBaseInstance(" << toHex(mode) << ", " << first << ", " << count << ", " << instancecount << ", " << baseinstance << ");" << TestLog::EndMessage;
	m_gl.drawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);
}

void CallLogWrapper::glDrawBuffer (glw::GLenum buf)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawBuffer(" << toHex(buf) << ");" << TestLog::EndMessage;
	m_gl.drawBuffer(buf);
}

void CallLogWrapper::glDrawBuffers (glw::GLsizei n, const glw::GLenum *bufs)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawBuffers(" << n << ", " << getEnumPointerStr(bufs, n, getDrawReadBufferName) << ");" << TestLog::EndMessage;
	m_gl.drawBuffers(n, bufs);
}

void CallLogWrapper::glDrawElements (glw::GLenum mode, glw::GLsizei count, glw::GLenum type, const void *indices)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElements(" << getPrimitiveTypeStr(mode) << ", " << count << ", " << getTypeStr(type) << ", " << indices << ");" << TestLog::EndMessage;
	m_gl.drawElements(mode, count, type, indices);
}

void CallLogWrapper::glDrawElementsBaseVertex (glw::GLenum mode, glw::GLsizei count, glw::GLenum type, const void *indices, glw::GLint basevertex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsBaseVertex(" << getPrimitiveTypeStr(mode) << ", " << count << ", " << getTypeStr(type) << ", " << indices << ", " << basevertex << ");" << TestLog::EndMessage;
	m_gl.drawElementsBaseVertex(mode, count, type, indices, basevertex);
}

void CallLogWrapper::glDrawElementsIndirect (glw::GLenum mode, glw::GLenum type, const void *indirect)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsIndirect(" << getPrimitiveTypeStr(mode) << ", " << getTypeStr(type) << ", " << indirect << ");" << TestLog::EndMessage;
	m_gl.drawElementsIndirect(mode, type, indirect);
}

void CallLogWrapper::glDrawElementsInstanced (glw::GLenum mode, glw::GLsizei count, glw::GLenum type, const void *indices, glw::GLsizei instancecount)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstanced(" << getPrimitiveTypeStr(mode) << ", " << count << ", " << getTypeStr(type) << ", " << indices << ", " << instancecount << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstanced(mode, count, type, indices, instancecount);
}

void CallLogWrapper::glDrawElementsInstancedBaseInstance (glw::GLenum mode, glw::GLsizei count, glw::GLenum type, const void *indices, glw::GLsizei instancecount, glw::GLuint baseinstance)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstancedBaseInstance(" << toHex(mode) << ", " << count << ", " << toHex(type) << ", " << indices << ", " << instancecount << ", " << baseinstance << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);
}

void CallLogWrapper::glDrawElementsInstancedBaseVertex (glw::GLenum mode, glw::GLsizei count, glw::GLenum type, const void *indices, glw::GLsizei instancecount, glw::GLint basevertex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstancedBaseVertex(" << getPrimitiveTypeStr(mode) << ", " << count << ", " << getTypeStr(type) << ", " << indices << ", " << instancecount << ", " << basevertex << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
}

void CallLogWrapper::glDrawElementsInstancedBaseVertexBaseInstance (glw::GLenum mode, glw::GLsizei count, glw::GLenum type, const void *indices, glw::GLsizei instancecount, glw::GLint basevertex, glw::GLuint baseinstance)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstancedBaseVertexBaseInstance(" << toHex(mode) << ", " << count << ", " << toHex(type) << ", " << indices << ", " << instancecount << ", " << basevertex << ", " << baseinstance << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);
}

void CallLogWrapper::glDrawRangeElements (glw::GLenum mode, glw::GLuint start, glw::GLuint end, glw::GLsizei count, glw::GLenum type, const void *indices)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawRangeElements(" << getPrimitiveTypeStr(mode) << ", " << start << ", " << end << ", " << count << ", " << getTypeStr(type) << ", " << indices << ");" << TestLog::EndMessage;
	m_gl.drawRangeElements(mode, start, end, count, type, indices);
}

void CallLogWrapper::glDrawRangeElementsBaseVertex (glw::GLenum mode, glw::GLuint start, glw::GLuint end, glw::GLsizei count, glw::GLenum type, const void *indices, glw::GLint basevertex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawRangeElementsBaseVertex(" << getPrimitiveTypeStr(mode) << ", " << start << ", " << end << ", " << count << ", " << getTypeStr(type) << ", " << indices << ", " << basevertex << ");" << TestLog::EndMessage;
	m_gl.drawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);
}

void CallLogWrapper::glDrawTransformFeedback (glw::GLenum mode, glw::GLuint id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedback(" << toHex(mode) << ", " << id << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedback(mode, id);
}

void CallLogWrapper::glDrawTransformFeedbackInstanced (glw::GLenum mode, glw::GLuint id, glw::GLsizei instancecount)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedbackInstanced(" << toHex(mode) << ", " << id << ", " << instancecount << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedbackInstanced(mode, id, instancecount);
}

void CallLogWrapper::glDrawTransformFeedbackStream (glw::GLenum mode, glw::GLuint id, glw::GLuint stream)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedbackStream(" << toHex(mode) << ", " << id << ", " << stream << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedbackStream(mode, id, stream);
}

void CallLogWrapper::glDrawTransformFeedbackStreamInstanced (glw::GLenum mode, glw::GLuint id, glw::GLuint stream, glw::GLsizei instancecount)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedbackStreamInstanced(" << toHex(mode) << ", " << id << ", " << stream << ", " << instancecount << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);
}

void CallLogWrapper::glEGLImageTargetRenderbufferStorageOES (glw::GLenum target, glw::GLeglImageOES image)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEGLImageTargetRenderbufferStorageOES(" << toHex(target) << ", " << image << ");" << TestLog::EndMessage;
	m_gl.eglImageTargetRenderbufferStorageOES(target, image);
}

void CallLogWrapper::glEGLImageTargetTexture2DOES (glw::GLenum target, glw::GLeglImageOES image)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEGLImageTargetTexture2DOES(" << toHex(target) << ", " << image << ");" << TestLog::EndMessage;
	m_gl.eglImageTargetTexture2DOES(target, image);
}

void CallLogWrapper::glEnable (glw::GLenum cap)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnable(" << getEnableCapStr(cap) << ");" << TestLog::EndMessage;
	m_gl.enable(cap);
}

void CallLogWrapper::glEnableClientStateIndexedEXT (glw::GLenum array, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableClientStateIndexedEXT(" << toHex(array) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.enableClientStateIndexedEXT(array, index);
}

void CallLogWrapper::glEnableClientStateiEXT (glw::GLenum array, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableClientStateiEXT(" << toHex(array) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.enableClientStateiEXT(array, index);
}

void CallLogWrapper::glEnableVertexArrayAttrib (glw::GLuint vaobj, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableVertexArrayAttrib(" << vaobj << ", " << index << ");" << TestLog::EndMessage;
	m_gl.enableVertexArrayAttrib(vaobj, index);
}

void CallLogWrapper::glEnableVertexArrayAttribEXT (glw::GLuint vaobj, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableVertexArrayAttribEXT(" << vaobj << ", " << index << ");" << TestLog::EndMessage;
	m_gl.enableVertexArrayAttribEXT(vaobj, index);
}

void CallLogWrapper::glEnableVertexArrayEXT (glw::GLuint vaobj, glw::GLenum array)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableVertexArrayEXT(" << vaobj << ", " << toHex(array) << ");" << TestLog::EndMessage;
	m_gl.enableVertexArrayEXT(vaobj, array);
}

void CallLogWrapper::glEnableVertexAttribArray (glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableVertexAttribArray(" << index << ");" << TestLog::EndMessage;
	m_gl.enableVertexAttribArray(index);
}

void CallLogWrapper::glEnablei (glw::GLenum target, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnablei(" << getIndexedEnableCapStr(target) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.enablei(target, index);
}

void CallLogWrapper::glEndConditionalRender (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndConditionalRender(" << ");" << TestLog::EndMessage;
	m_gl.endConditionalRender();
}

void CallLogWrapper::glEndQuery (glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndQuery(" << getQueryTargetStr(target) << ");" << TestLog::EndMessage;
	m_gl.endQuery(target);
}

void CallLogWrapper::glEndQueryIndexed (glw::GLenum target, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndQueryIndexed(" << toHex(target) << ", " << index << ");" << TestLog::EndMessage;
	m_gl.endQueryIndexed(target, index);
}

void CallLogWrapper::glEndTransformFeedback (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndTransformFeedback(" << ");" << TestLog::EndMessage;
	m_gl.endTransformFeedback();
}

glw::GLsync CallLogWrapper::glFenceSync (glw::GLenum condition, glw::GLbitfield flags)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFenceSync(" << toHex(condition) << ", " << toHex(flags) << ");" << TestLog::EndMessage;
	glw::GLsync returnValue = m_gl.fenceSync(condition, flags);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glFinish (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFinish(" << ");" << TestLog::EndMessage;
	m_gl.finish();
}

void CallLogWrapper::glFlush (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFlush(" << ");" << TestLog::EndMessage;
	m_gl.flush();
}

void CallLogWrapper::glFlushMappedBufferRange (glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFlushMappedBufferRange(" << getBufferTargetStr(target) << ", " << offset << ", " << length << ");" << TestLog::EndMessage;
	m_gl.flushMappedBufferRange(target, offset, length);
}

void CallLogWrapper::glFlushMappedNamedBufferRange (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFlushMappedNamedBufferRange(" << buffer << ", " << offset << ", " << length << ");" << TestLog::EndMessage;
	m_gl.flushMappedNamedBufferRange(buffer, offset, length);
}

void CallLogWrapper::glFlushMappedNamedBufferRangeEXT (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFlushMappedNamedBufferRangeEXT(" << buffer << ", " << offset << ", " << length << ");" << TestLog::EndMessage;
	m_gl.flushMappedNamedBufferRangeEXT(buffer, offset, length);
}

void CallLogWrapper::glFramebufferDrawBufferEXT (glw::GLuint framebuffer, glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferDrawBufferEXT(" << framebuffer << ", " << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.framebufferDrawBufferEXT(framebuffer, mode);
}

void CallLogWrapper::glFramebufferDrawBuffersEXT (glw::GLuint framebuffer, glw::GLsizei n, const glw::GLenum *bufs)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferDrawBuffersEXT(" << framebuffer << ", " << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(bufs))) << ");" << TestLog::EndMessage;
	m_gl.framebufferDrawBuffersEXT(framebuffer, n, bufs);
}

void CallLogWrapper::glFramebufferParameteri (glw::GLenum target, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferParameteri(" << getFramebufferTargetStr(target) << ", " << getFramebufferParameterStr(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.framebufferParameteri(target, pname, param);
}

void CallLogWrapper::glFramebufferReadBufferEXT (glw::GLuint framebuffer, glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferReadBufferEXT(" << framebuffer << ", " << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.framebufferReadBufferEXT(framebuffer, mode);
}

void CallLogWrapper::glFramebufferRenderbuffer (glw::GLenum target, glw::GLenum attachment, glw::GLenum renderbuffertarget, glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferRenderbuffer(" << getFramebufferTargetStr(target) << ", " << getFramebufferAttachmentStr(attachment) << ", " << getFramebufferTargetStr(renderbuffertarget) << ", " << renderbuffer << ");" << TestLog::EndMessage;
	m_gl.framebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void CallLogWrapper::glFramebufferShadingRateEXT (glw::GLenum target, glw::GLenum attachment, glw::GLuint texture, glw::GLint baseLayer, glw::GLsizei numLayers, glw::GLsizei texelWidth, glw::GLsizei texelHeight)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferShadingRateEXT(" << toHex(target) << ", " << toHex(attachment) << ", " << texture << ", " << baseLayer << ", " << numLayers << ", " << texelWidth << ", " << texelHeight << ");" << TestLog::EndMessage;
	m_gl.framebufferShadingRateEXT(target, attachment, texture, baseLayer, numLayers, texelWidth, texelHeight);
}

void CallLogWrapper::glFramebufferTexture (glw::GLenum target, glw::GLenum attachment, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture(" << getFramebufferTargetStr(target) << ", " << getFramebufferAttachmentStr(attachment) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture(target, attachment, texture, level);
}

void CallLogWrapper::glFramebufferTexture1D (glw::GLenum target, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture1D(" << toHex(target) << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture1D(target, attachment, textarget, texture, level);
}

void CallLogWrapper::glFramebufferTexture2D (glw::GLenum target, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture2D(" << getFramebufferTargetStr(target) << ", " << getFramebufferAttachmentStr(attachment) << ", " << getTextureTargetStr(textarget) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture2D(target, attachment, textarget, texture, level);
}

void CallLogWrapper::glFramebufferTexture2DMultisampleEXT (glw::GLenum target, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level, glw::GLsizei samples)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture2DMultisampleEXT(" << toHex(target) << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ", " << samples << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture2DMultisampleEXT(target, attachment, textarget, texture, level, samples);
}

void CallLogWrapper::glFramebufferTexture3D (glw::GLenum target, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level, glw::GLint zoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture3D(" << toHex(target) << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ", " << zoffset << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
}

void CallLogWrapper::glFramebufferTexture3DOES (glw::GLenum target, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level, glw::GLint zoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture3DOES(" << toHex(target) << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ", " << zoffset << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture3DOES(target, attachment, textarget, texture, level, zoffset);
}

void CallLogWrapper::glFramebufferTextureLayer (glw::GLenum target, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLint layer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTextureLayer(" << getFramebufferTargetStr(target) << ", " << getFramebufferAttachmentStr(attachment) << ", " << texture << ", " << level << ", " << layer << ");" << TestLog::EndMessage;
	m_gl.framebufferTextureLayer(target, attachment, texture, level, layer);
}

void CallLogWrapper::glFramebufferTextureMultisampleMultiviewOVR (glw::GLenum target, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLsizei samples, glw::GLint baseViewIndex, glw::GLsizei numViews)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTextureMultisampleMultiviewOVR(" << toHex(target) << ", " << toHex(attachment) << ", " << texture << ", " << level << ", " << samples << ", " << baseViewIndex << ", " << numViews << ");" << TestLog::EndMessage;
	m_gl.framebufferTextureMultisampleMultiviewOVR(target, attachment, texture, level, samples, baseViewIndex, numViews);
}

void CallLogWrapper::glFramebufferTextureMultiviewOVR (glw::GLenum target, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLint baseViewIndex, glw::GLsizei numViews)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTextureMultiviewOVR(" << toHex(target) << ", " << toHex(attachment) << ", " << texture << ", " << level << ", " << baseViewIndex << ", " << numViews << ");" << TestLog::EndMessage;
	m_gl.framebufferTextureMultiviewOVR(target, attachment, texture, level, baseViewIndex, numViews);
}

void CallLogWrapper::glFrontFace (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFrontFace(" << getWindingStr(mode) << ");" << TestLog::EndMessage;
	m_gl.frontFace(mode);
}

void CallLogWrapper::glGenBuffers (glw::GLsizei n, glw::GLuint *buffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenBuffers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(buffers))) << ");" << TestLog::EndMessage;
	m_gl.genBuffers(n, buffers);
	if (m_enableLog)
		m_log << TestLog::Message << "// buffers = " << getPointerStr(buffers, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenFramebuffers (glw::GLsizei n, glw::GLuint *framebuffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenFramebuffers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(framebuffers))) << ");" << TestLog::EndMessage;
	m_gl.genFramebuffers(n, framebuffers);
	if (m_enableLog)
		m_log << TestLog::Message << "// framebuffers = " << getPointerStr(framebuffers, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenProgramPipelines (glw::GLsizei n, glw::GLuint *pipelines)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenProgramPipelines(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(pipelines))) << ");" << TestLog::EndMessage;
	m_gl.genProgramPipelines(n, pipelines);
	if (m_enableLog)
		m_log << TestLog::Message << "// pipelines = " << getPointerStr(pipelines, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenQueries (glw::GLsizei n, glw::GLuint *ids)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenQueries(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(ids))) << ");" << TestLog::EndMessage;
	m_gl.genQueries(n, ids);
	if (m_enableLog)
		m_log << TestLog::Message << "// ids = " << getPointerStr(ids, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenRenderbuffers (glw::GLsizei n, glw::GLuint *renderbuffers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenRenderbuffers(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(renderbuffers))) << ");" << TestLog::EndMessage;
	m_gl.genRenderbuffers(n, renderbuffers);
	if (m_enableLog)
		m_log << TestLog::Message << "// renderbuffers = " << getPointerStr(renderbuffers, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenSamplers (glw::GLsizei count, glw::GLuint *samplers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenSamplers(" << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(samplers))) << ");" << TestLog::EndMessage;
	m_gl.genSamplers(count, samplers);
}

void CallLogWrapper::glGenTextures (glw::GLsizei n, glw::GLuint *textures)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenTextures(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(textures))) << ");" << TestLog::EndMessage;
	m_gl.genTextures(n, textures);
	if (m_enableLog)
		m_log << TestLog::Message << "// textures = " << getPointerStr(textures, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenTransformFeedbacks (glw::GLsizei n, glw::GLuint *ids)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenTransformFeedbacks(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(ids))) << ");" << TestLog::EndMessage;
	m_gl.genTransformFeedbacks(n, ids);
	if (m_enableLog)
		m_log << TestLog::Message << "// ids = " << getPointerStr(ids, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenVertexArrays (glw::GLsizei n, glw::GLuint *arrays)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenVertexArrays(" << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(arrays))) << ");" << TestLog::EndMessage;
	m_gl.genVertexArrays(n, arrays);
	if (m_enableLog)
		m_log << TestLog::Message << "// arrays = " << getPointerStr(arrays, n) << TestLog::EndMessage;
}

void CallLogWrapper::glGenerateMipmap (glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenerateMipmap(" << getTextureTargetStr(target) << ");" << TestLog::EndMessage;
	m_gl.generateMipmap(target);
}

void CallLogWrapper::glGenerateMultiTexMipmapEXT (glw::GLenum texunit, glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenerateMultiTexMipmapEXT(" << toHex(texunit) << ", " << toHex(target) << ");" << TestLog::EndMessage;
	m_gl.generateMultiTexMipmapEXT(texunit, target);
}

void CallLogWrapper::glGenerateTextureMipmap (glw::GLuint texture)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenerateTextureMipmap(" << texture << ");" << TestLog::EndMessage;
	m_gl.generateTextureMipmap(texture);
}

void CallLogWrapper::glGenerateTextureMipmapEXT (glw::GLuint texture, glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenerateTextureMipmapEXT(" << texture << ", " << toHex(target) << ");" << TestLog::EndMessage;
	m_gl.generateTextureMipmapEXT(texture, target);
}

void CallLogWrapper::glGetActiveAtomicCounterBufferiv (glw::GLuint program, glw::GLuint bufferIndex, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveAtomicCounterBufferiv(" << program << ", " << bufferIndex << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getActiveAtomicCounterBufferiv(program, bufferIndex, pname, params);
}

void CallLogWrapper::glGetActiveAttrib (glw::GLuint program, glw::GLuint index, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLint *size, glw::GLenum *type, glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveAttrib(" << program << ", " << index << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(size))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(type))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(name))) << ");" << TestLog::EndMessage;
	m_gl.getActiveAttrib(program, index, bufSize, length, size, type, name);
}

void CallLogWrapper::glGetActiveSubroutineName (glw::GLuint program, glw::GLenum shadertype, glw::GLuint index, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveSubroutineName(" << program << ", " << toHex(shadertype) << ", " << index << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(name))) << ");" << TestLog::EndMessage;
	m_gl.getActiveSubroutineName(program, shadertype, index, bufSize, length, name);
}

void CallLogWrapper::glGetActiveSubroutineUniformName (glw::GLuint program, glw::GLenum shadertype, glw::GLuint index, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveSubroutineUniformName(" << program << ", " << toHex(shadertype) << ", " << index << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(name))) << ");" << TestLog::EndMessage;
	m_gl.getActiveSubroutineUniformName(program, shadertype, index, bufSize, length, name);
}

void CallLogWrapper::glGetActiveSubroutineUniformiv (glw::GLuint program, glw::GLenum shadertype, glw::GLuint index, glw::GLenum pname, glw::GLint *values)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveSubroutineUniformiv(" << program << ", " << toHex(shadertype) << ", " << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(values))) << ");" << TestLog::EndMessage;
	m_gl.getActiveSubroutineUniformiv(program, shadertype, index, pname, values);
}

void CallLogWrapper::glGetActiveUniform (glw::GLuint program, glw::GLuint index, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLint *size, glw::GLenum *type, glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniform(" << program << ", " << index << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(size))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(type))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(name))) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniform(program, index, bufSize, length, size, type, name);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// length = " << getPointerStr(length, 1) << TestLog::EndMessage;
		m_log << TestLog::Message << "// size = " << getPointerStr(size, 1) << TestLog::EndMessage;
		m_log << TestLog::Message << "// type = " << getEnumPointerStr(type, 1, getShaderVarTypeName) << TestLog::EndMessage;
		m_log << TestLog::Message << "// name = " << getStringStr(name) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetActiveUniformBlockName (glw::GLuint program, glw::GLuint uniformBlockIndex, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *uniformBlockName)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformBlockName(" << program << ", " << uniformBlockIndex << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(uniformBlockName))) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void CallLogWrapper::glGetActiveUniformBlockiv (glw::GLuint program, glw::GLuint uniformBlockIndex, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformBlockiv(" << program << ", " << uniformBlockIndex << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void CallLogWrapper::glGetActiveUniformName (glw::GLuint program, glw::GLuint uniformIndex, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *uniformName)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformName(" << program << ", " << uniformIndex << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(uniformName))) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
}

void CallLogWrapper::glGetActiveUniformsiv (glw::GLuint program, glw::GLsizei uniformCount, const glw::GLuint *uniformIndices, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformsiv(" << program << ", " << uniformCount << ", " << getPointerStr(uniformIndices, uniformCount) << ", " << getUniformParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, uniformCount) << TestLog::EndMessage;
}

void CallLogWrapper::glGetAttachedShaders (glw::GLuint program, glw::GLsizei maxCount, glw::GLsizei *count, glw::GLuint *shaders)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetAttachedShaders(" << program << ", " << maxCount << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(count))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(shaders))) << ");" << TestLog::EndMessage;
	m_gl.getAttachedShaders(program, maxCount, count, shaders);
}

glw::GLint CallLogWrapper::glGetAttribLocation (glw::GLuint program, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetAttribLocation(" << program << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getAttribLocation(program, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetBooleani_v (glw::GLenum target, glw::GLuint index, glw::GLboolean *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBooleani_v(" << getGettableIndexedStateStr(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getBooleani_v(target, index, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getBooleanPointerStr(data, getIndexedQueryNumArgsOut(target)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetBooleanv (glw::GLenum pname, glw::GLboolean *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBooleanv(" << getGettableStateStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getBooleanv(pname, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getBooleanPointerStr(data, getBasicQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetBufferParameteri64v (glw::GLenum target, glw::GLenum pname, glw::GLint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferParameteri64v(" << getBufferTargetStr(target) << ", " << getBufferQueryStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getBufferParameteri64v(target, pname, params);
}

void CallLogWrapper::glGetBufferParameteriv (glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferParameteriv(" << getBufferTargetStr(target) << ", " << getBufferQueryStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getBufferParameteriv(target, pname, params);
}

void CallLogWrapper::glGetBufferPointerv (glw::GLenum target, glw::GLenum pname, void **params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferPointerv(" << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getBufferPointerv(target, pname, params);
}

void CallLogWrapper::glGetBufferSubData (glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr size, void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferSubData(" << toHex(target) << ", " << offset << ", " << size << ", " << data << ");" << TestLog::EndMessage;
	m_gl.getBufferSubData(target, offset, size, data);
}

void CallLogWrapper::glGetCompressedMultiTexImageEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint lod, void *img)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetCompressedMultiTexImageEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << lod << ", " << img << ");" << TestLog::EndMessage;
	m_gl.getCompressedMultiTexImageEXT(texunit, target, lod, img);
}

void CallLogWrapper::glGetCompressedTexImage (glw::GLenum target, glw::GLint level, void *img)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetCompressedTexImage(" << toHex(target) << ", " << level << ", " << img << ");" << TestLog::EndMessage;
	m_gl.getCompressedTexImage(target, level, img);
}

void CallLogWrapper::glGetCompressedTextureImage (glw::GLuint texture, glw::GLint level, glw::GLsizei bufSize, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetCompressedTextureImage(" << texture << ", " << level << ", " << bufSize << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getCompressedTextureImage(texture, level, bufSize, pixels);
}

void CallLogWrapper::glGetCompressedTextureImageEXT (glw::GLuint texture, glw::GLenum target, glw::GLint lod, void *img)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetCompressedTextureImageEXT(" << texture << ", " << toHex(target) << ", " << lod << ", " << img << ");" << TestLog::EndMessage;
	m_gl.getCompressedTextureImageEXT(texture, target, lod, img);
}

void CallLogWrapper::glGetCompressedTextureSubImage (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLsizei bufSize, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetCompressedTextureSubImage(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << bufSize << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getCompressedTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, bufSize, pixels);
}

glw::GLuint CallLogWrapper::glGetDebugMessageLog (glw::GLuint count, glw::GLsizei bufSize, glw::GLenum *sources, glw::GLenum *types, glw::GLuint *ids, glw::GLenum *severities, glw::GLsizei *lengths, glw::GLchar *messageLog)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetDebugMessageLog(" << count << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(sources))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(types))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(ids))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(severities))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(lengths))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(messageLog))) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetDoublei_v (glw::GLenum target, glw::GLuint index, glw::GLdouble *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetDoublei_v(" << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getDoublei_v(target, index, data);
}

void CallLogWrapper::glGetDoublev (glw::GLenum pname, glw::GLdouble *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetDoublev(" << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getDoublev(pname, data);
}

glw::GLenum CallLogWrapper::glGetError (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetError(" << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.getError();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getErrorStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetFloati_v (glw::GLenum target, glw::GLuint index, glw::GLfloat *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFloati_v(" << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getFloati_v(target, index, data);
}

void CallLogWrapper::glGetFloatv (glw::GLenum pname, glw::GLfloat *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFloatv(" << getGettableStateStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getFloatv(pname, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getPointerStr(data, getBasicQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

glw::GLint CallLogWrapper::glGetFragDataIndex (glw::GLuint program, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFragDataIndex(" << program << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getFragDataIndex(program, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLint CallLogWrapper::glGetFragDataLocation (glw::GLuint program, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFragDataLocation(" << program << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getFragDataLocation(program, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetFragmentShadingRatesEXT (glw::GLsizei samples, glw::GLsizei maxCount, glw::GLsizei *count, glw::GLenum *shadingRates)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFragmentShadingRatesEXT(" << samples << ", " << maxCount << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(count))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(shadingRates))) << ");" << TestLog::EndMessage;
	m_gl.getFragmentShadingRatesEXT(samples, maxCount, count, shadingRates);
}

void CallLogWrapper::glGetFramebufferAttachmentParameteriv (glw::GLenum target, glw::GLenum attachment, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFramebufferAttachmentParameteriv(" << getFramebufferTargetStr(target) << ", " << getFramebufferAttachmentStr(attachment) << ", " << getFramebufferAttachmentParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getFramebufferAttachmentParameteriv(target, attachment, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getFramebufferAttachmentParameterValueStr(pname, params) << TestLog::EndMessage;
}

void CallLogWrapper::glGetFramebufferParameteriv (glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFramebufferParameteriv(" << getFramebufferTargetStr(target) << ", " << getFramebufferParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getFramebufferParameteriv(target, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetFramebufferParameterivEXT (glw::GLuint framebuffer, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFramebufferParameterivEXT(" << framebuffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getFramebufferParameterivEXT(framebuffer, pname, params);
}

glw::GLenum CallLogWrapper::glGetGraphicsResetStatus (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetGraphicsResetStatus(" << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.getGraphicsResetStatus();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetInteger64i_v (glw::GLenum target, glw::GLuint index, glw::GLint64 *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInteger64i_v(" << getGettableIndexedStateStr(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getInteger64i_v(target, index, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getPointerStr(data, getIndexedQueryNumArgsOut(target)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetInteger64v (glw::GLenum pname, glw::GLint64 *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInteger64v(" << getGettableStateStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getInteger64v(pname, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getPointerStr(data, getBasicQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetIntegeri_v (glw::GLenum target, glw::GLuint index, glw::GLint *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetIntegeri_v(" << getGettableIndexedStateStr(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getIntegeri_v(target, index, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getPointerStr(data, getIndexedQueryNumArgsOut(target)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetIntegerv (glw::GLenum pname, glw::GLint *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetIntegerv(" << getGettableStateStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getIntegerv(pname, data);
	if (m_enableLog)
		m_log << TestLog::Message << "// data = " << getPointerStr(data, getBasicQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetInternalformatSampleivNV (glw::GLenum target, glw::GLenum internalformat, glw::GLsizei samples, glw::GLenum pname, glw::GLsizei count, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInternalformatSampleivNV(" << toHex(target) << ", " << toHex(internalformat) << ", " << samples << ", " << toHex(pname) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getInternalformatSampleivNV(target, internalformat, samples, pname, count, params);
}

void CallLogWrapper::glGetInternalformati64v (glw::GLenum target, glw::GLenum internalformat, glw::GLenum pname, glw::GLsizei count, glw::GLint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInternalformati64v(" << toHex(target) << ", " << toHex(internalformat) << ", " << toHex(pname) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getInternalformati64v(target, internalformat, pname, count, params);
}

void CallLogWrapper::glGetInternalformativ (glw::GLenum target, glw::GLenum internalformat, glw::GLenum pname, glw::GLsizei count, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInternalformativ(" << getInternalFormatTargetStr(target) << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << getInternalFormatParameterStr(pname) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getInternalformativ(target, internalformat, pname, count, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, count) << TestLog::EndMessage;
}

void CallLogWrapper::glGetMultiTexEnvfvEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexEnvfvEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexEnvfvEXT(texunit, target, pname, params);
}

void CallLogWrapper::glGetMultiTexEnvivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexEnvivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexEnvivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glGetMultiTexGendvEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexGendvEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexGendvEXT(texunit, coord, pname, params);
}

void CallLogWrapper::glGetMultiTexGenfvEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexGenfvEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexGenfvEXT(texunit, coord, pname, params);
}

void CallLogWrapper::glGetMultiTexGenivEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexGenivEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexGenivEXT(texunit, coord, pname, params);
}

void CallLogWrapper::glGetMultiTexImageEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum format, glw::GLenum type, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexImageEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getMultiTexImageEXT(texunit, target, level, format, type, pixels);
}

void CallLogWrapper::glGetMultiTexLevelParameterfvEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexLevelParameterfvEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexLevelParameterfvEXT(texunit, target, level, pname, params);
}

void CallLogWrapper::glGetMultiTexLevelParameterivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexLevelParameterivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexLevelParameterivEXT(texunit, target, level, pname, params);
}

void CallLogWrapper::glGetMultiTexParameterIivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexParameterIivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexParameterIivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glGetMultiTexParameterIuivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexParameterIuivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexParameterIuivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glGetMultiTexParameterfvEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexParameterfvEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexParameterfvEXT(texunit, target, pname, params);
}

void CallLogWrapper::glGetMultiTexParameterivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultiTexParameterivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getMultiTexParameterivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glGetMultisamplefv (glw::GLenum pname, glw::GLuint index, glw::GLfloat *val)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultisamplefv(" << getMultisampleParameterStr(pname) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(val))) << ");" << TestLog::EndMessage;
	m_gl.getMultisamplefv(pname, index, val);
	if (m_enableLog)
		m_log << TestLog::Message << "// val = " << getPointerStr(val, 2) << TestLog::EndMessage;
}

void CallLogWrapper::glGetNamedBufferParameteri64v (glw::GLuint buffer, glw::GLenum pname, glw::GLint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferParameteri64v(" << buffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferParameteri64v(buffer, pname, params);
}

void CallLogWrapper::glGetNamedBufferParameteriv (glw::GLuint buffer, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferParameteriv(" << buffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferParameteriv(buffer, pname, params);
}

void CallLogWrapper::glGetNamedBufferParameterivEXT (glw::GLuint buffer, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferParameterivEXT(" << buffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferParameterivEXT(buffer, pname, params);
}

void CallLogWrapper::glGetNamedBufferPointerv (glw::GLuint buffer, glw::GLenum pname, void **params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferPointerv(" << buffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferPointerv(buffer, pname, params);
}

void CallLogWrapper::glGetNamedBufferPointervEXT (glw::GLuint buffer, glw::GLenum pname, void **params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferPointervEXT(" << buffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferPointervEXT(buffer, pname, params);
}

void CallLogWrapper::glGetNamedBufferSubData (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size, void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferSubData(" << buffer << ", " << offset << ", " << size << ", " << data << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferSubData(buffer, offset, size, data);
}

void CallLogWrapper::glGetNamedBufferSubDataEXT (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size, void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedBufferSubDataEXT(" << buffer << ", " << offset << ", " << size << ", " << data << ");" << TestLog::EndMessage;
	m_gl.getNamedBufferSubDataEXT(buffer, offset, size, data);
}

void CallLogWrapper::glGetNamedFramebufferAttachmentParameteriv (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedFramebufferAttachmentParameteriv(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedFramebufferAttachmentParameteriv(framebuffer, attachment, pname, params);
}

void CallLogWrapper::glGetNamedFramebufferAttachmentParameterivEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedFramebufferAttachmentParameterivEXT(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment, pname, params);
}

void CallLogWrapper::glGetNamedFramebufferParameteriv (glw::GLuint framebuffer, glw::GLenum pname, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedFramebufferParameteriv(" << framebuffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getNamedFramebufferParameteriv(framebuffer, pname, param);
}

void CallLogWrapper::glGetNamedFramebufferParameterivEXT (glw::GLuint framebuffer, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedFramebufferParameterivEXT(" << framebuffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedFramebufferParameterivEXT(framebuffer, pname, params);
}

void CallLogWrapper::glGetNamedProgramLocalParameterIivEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedProgramLocalParameterIivEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedProgramLocalParameterIivEXT(program, target, index, params);
}

void CallLogWrapper::glGetNamedProgramLocalParameterIuivEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedProgramLocalParameterIuivEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedProgramLocalParameterIuivEXT(program, target, index, params);
}

void CallLogWrapper::glGetNamedProgramLocalParameterdvEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedProgramLocalParameterdvEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedProgramLocalParameterdvEXT(program, target, index, params);
}

void CallLogWrapper::glGetNamedProgramLocalParameterfvEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedProgramLocalParameterfvEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedProgramLocalParameterfvEXT(program, target, index, params);
}

void CallLogWrapper::glGetNamedProgramStringEXT (glw::GLuint program, glw::GLenum target, glw::GLenum pname, void *string)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedProgramStringEXT(" << program << ", " << toHex(target) << ", " << toHex(pname) << ", " << string << ");" << TestLog::EndMessage;
	m_gl.getNamedProgramStringEXT(program, target, pname, string);
}

void CallLogWrapper::glGetNamedProgramivEXT (glw::GLuint program, glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedProgramivEXT(" << program << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedProgramivEXT(program, target, pname, params);
}

void CallLogWrapper::glGetNamedRenderbufferParameteriv (glw::GLuint renderbuffer, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedRenderbufferParameteriv(" << renderbuffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedRenderbufferParameteriv(renderbuffer, pname, params);
}

void CallLogWrapper::glGetNamedRenderbufferParameterivEXT (glw::GLuint renderbuffer, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetNamedRenderbufferParameterivEXT(" << renderbuffer << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getNamedRenderbufferParameterivEXT(renderbuffer, pname, params);
}

void CallLogWrapper::glGetObjectLabel (glw::GLenum identifier, glw::GLuint name, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *label)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetObjectLabel(" << toHex(identifier) << ", " << name << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(label))) << ");" << TestLog::EndMessage;
	m_gl.getObjectLabel(identifier, name, bufSize, length, label);
}

void CallLogWrapper::glGetObjectPtrLabel (const void *ptr, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *label)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetObjectPtrLabel(" << ptr << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(label))) << ");" << TestLog::EndMessage;
	m_gl.getObjectPtrLabel(ptr, bufSize, length, label);
}

void CallLogWrapper::glGetPointerIndexedvEXT (glw::GLenum target, glw::GLuint index, void **data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetPointerIndexedvEXT(" << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(data))) << ");" << TestLog::EndMessage;
	m_gl.getPointerIndexedvEXT(target, index, data);
}

void CallLogWrapper::glGetPointeri_vEXT (glw::GLenum pname, glw::GLuint index, void **params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetPointeri_vEXT(" << toHex(pname) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getPointeri_vEXT(pname, index, params);
}

void CallLogWrapper::glGetPointerv (glw::GLenum pname, void **params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetPointerv(" << getPointerStateStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getPointerv(pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetProgramBinary (glw::GLuint program, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLenum *binaryFormat, void *binary)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramBinary(" << program << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(binaryFormat))) << ", " << binary << ");" << TestLog::EndMessage;
	m_gl.getProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void CallLogWrapper::glGetProgramInfoLog (glw::GLuint program, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *infoLog)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramInfoLog(" << program << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(infoLog))) << ");" << TestLog::EndMessage;
	m_gl.getProgramInfoLog(program, bufSize, length, infoLog);
	if (m_enableLog)
		m_log << TestLog::Message << "// length = " << getPointerStr(length, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetProgramInterfaceiv (glw::GLuint program, glw::GLenum programInterface, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramInterfaceiv(" << program << ", " << toHex(programInterface) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getProgramInterfaceiv(program, programInterface, pname, params);
}

void CallLogWrapper::glGetProgramPipelineInfoLog (glw::GLuint pipeline, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *infoLog)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramPipelineInfoLog(" << pipeline << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(infoLog))) << ");" << TestLog::EndMessage;
	m_gl.getProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
	if (m_enableLog)
		m_log << TestLog::Message << "// length = " << getPointerStr(length, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetProgramPipelineiv (glw::GLuint pipeline, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramPipelineiv(" << pipeline << ", " << getPipelineParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getProgramPipelineiv(pipeline, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

glw::GLuint CallLogWrapper::glGetProgramResourceIndex (glw::GLuint program, glw::GLenum programInterface, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceIndex(" << program << ", " << getProgramInterfaceStr(programInterface) << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getProgramResourceIndex(program, programInterface, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLint CallLogWrapper::glGetProgramResourceLocation (glw::GLuint program, glw::GLenum programInterface, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceLocation(" << program << ", " << toHex(programInterface) << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getProgramResourceLocation(program, programInterface, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLint CallLogWrapper::glGetProgramResourceLocationIndex (glw::GLuint program, glw::GLenum programInterface, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceLocationIndex(" << program << ", " << toHex(programInterface) << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getProgramResourceLocationIndex(program, programInterface, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetProgramResourceName (glw::GLuint program, glw::GLenum programInterface, glw::GLuint index, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceName(" << program << ", " << toHex(programInterface) << ", " << index << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(name))) << ");" << TestLog::EndMessage;
	m_gl.getProgramResourceName(program, programInterface, index, bufSize, length, name);
}

void CallLogWrapper::glGetProgramResourceiv (glw::GLuint program, glw::GLenum programInterface, glw::GLuint index, glw::GLsizei propCount, const glw::GLenum *props, glw::GLsizei count, glw::GLsizei *length, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceiv(" << program << ", " << getProgramInterfaceStr(programInterface) << ", " << index << ", " << propCount << ", " << getEnumPointerStr(props, propCount, getProgramResourcePropertyName) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getProgramResourceiv(program, programInterface, index, propCount, props, count, length, params);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// length = " << getPointerStr(length, 1) << TestLog::EndMessage;
		m_log << TestLog::Message << "// params = " << getPointerStr(params, ((length == nullptr) ? (count) : ((count < *length) ? (count) : (*length)))) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetProgramStageiv (glw::GLuint program, glw::GLenum shadertype, glw::GLenum pname, glw::GLint *values)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramStageiv(" << program << ", " << toHex(shadertype) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(values))) << ");" << TestLog::EndMessage;
	m_gl.getProgramStageiv(program, shadertype, pname, values);
}

void CallLogWrapper::glGetProgramiv (glw::GLuint program, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramiv(" << program << ", " << getProgramParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getProgramiv(program, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getProgramQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetQueryBufferObjecti64v (glw::GLuint id, glw::GLuint buffer, glw::GLenum pname, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryBufferObjecti64v(" << id << ", " << buffer << ", " << toHex(pname) << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.getQueryBufferObjecti64v(id, buffer, pname, offset);
}

void CallLogWrapper::glGetQueryBufferObjectiv (glw::GLuint id, glw::GLuint buffer, glw::GLenum pname, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryBufferObjectiv(" << id << ", " << buffer << ", " << toHex(pname) << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.getQueryBufferObjectiv(id, buffer, pname, offset);
}

void CallLogWrapper::glGetQueryBufferObjectui64v (glw::GLuint id, glw::GLuint buffer, glw::GLenum pname, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryBufferObjectui64v(" << id << ", " << buffer << ", " << toHex(pname) << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.getQueryBufferObjectui64v(id, buffer, pname, offset);
}

void CallLogWrapper::glGetQueryBufferObjectuiv (glw::GLuint id, glw::GLuint buffer, glw::GLenum pname, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryBufferObjectuiv(" << id << ", " << buffer << ", " << toHex(pname) << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.getQueryBufferObjectuiv(id, buffer, pname, offset);
}

void CallLogWrapper::glGetQueryIndexediv (glw::GLenum target, glw::GLuint index, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryIndexediv(" << toHex(target) << ", " << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getQueryIndexediv(target, index, pname, params);
}

void CallLogWrapper::glGetQueryObjecti64v (glw::GLuint id, glw::GLenum pname, glw::GLint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjecti64v(" << id << ", " << getQueryObjectParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjecti64v(id, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetQueryObjectiv (glw::GLuint id, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjectiv(" << id << ", " << getQueryObjectParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjectiv(id, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetQueryObjectui64v (glw::GLuint id, glw::GLenum pname, glw::GLuint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjectui64v(" << id << ", " << getQueryObjectParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjectui64v(id, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetQueryObjectuiv (glw::GLuint id, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjectuiv(" << id << ", " << getQueryObjectParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjectuiv(id, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetQueryiv (glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryiv(" << getQueryTargetStr(target) << ", " << getQueryParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getQueryiv(target, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetRenderbufferParameteriv (glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetRenderbufferParameteriv(" << getFramebufferTargetStr(target) << ", " << getRenderbufferParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getRenderbufferParameteriv(target, pname, params);
}

void CallLogWrapper::glGetSamplerParameterIiv (glw::GLuint sampler, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameterIiv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameterIiv(sampler, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetSamplerParameterIuiv (glw::GLuint sampler, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameterIuiv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameterIuiv(sampler, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetSamplerParameterfv (glw::GLuint sampler, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameterfv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameterfv(sampler, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetSamplerParameteriv (glw::GLuint sampler, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameteriv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameteriv(sampler, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetShaderInfoLog (glw::GLuint shader, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *infoLog)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderInfoLog(" << shader << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(infoLog))) << ");" << TestLog::EndMessage;
	m_gl.getShaderInfoLog(shader, bufSize, length, infoLog);
	if (m_enableLog)
		m_log << TestLog::Message << "// length = " << getPointerStr(length, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetShaderPrecisionFormat (glw::GLenum shadertype, glw::GLenum precisiontype, glw::GLint *range, glw::GLint *precision)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderPrecisionFormat(" << getShaderTypeStr(shadertype) << ", " << getPrecisionFormatTypeStr(precisiontype) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(range))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(precision))) << ");" << TestLog::EndMessage;
	m_gl.getShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void CallLogWrapper::glGetShaderSource (glw::GLuint shader, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLchar *source)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderSource(" << shader << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(source))) << ");" << TestLog::EndMessage;
	m_gl.getShaderSource(shader, bufSize, length, source);
}

void CallLogWrapper::glGetShaderiv (glw::GLuint shader, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderiv(" << shader << ", " << getShaderParamStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getShaderiv(shader, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

const glw::GLubyte * CallLogWrapper::glGetString (glw::GLenum name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetString(" << getGettableStringStr(name) << ");" << TestLog::EndMessage;
	const glw::GLubyte * returnValue = m_gl.getString(name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

const glw::GLubyte * CallLogWrapper::glGetStringi (glw::GLenum name, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetStringi(" << getGettableStringStr(name) << ", " << index << ");" << TestLog::EndMessage;
	const glw::GLubyte * returnValue = m_gl.getStringi(name, index);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLuint CallLogWrapper::glGetSubroutineIndex (glw::GLuint program, glw::GLenum shadertype, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSubroutineIndex(" << program << ", " << toHex(shadertype) << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getSubroutineIndex(program, shadertype, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLint CallLogWrapper::glGetSubroutineUniformLocation (glw::GLuint program, glw::GLenum shadertype, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSubroutineUniformLocation(" << program << ", " << toHex(shadertype) << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getSubroutineUniformLocation(program, shadertype, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetSynciv (glw::GLsync sync, glw::GLenum pname, glw::GLsizei count, glw::GLsizei *length, glw::GLint *values)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSynciv(" << sync << ", " << toHex(pname) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(values))) << ");" << TestLog::EndMessage;
	m_gl.getSynciv(sync, pname, count, length, values);
}

void CallLogWrapper::glGetTexImage (glw::GLenum target, glw::GLint level, glw::GLenum format, glw::GLenum type, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexImage(" << toHex(target) << ", " << level << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getTexImage(target, level, format, type, pixels);
}

void CallLogWrapper::glGetTexLevelParameterfv (glw::GLenum target, glw::GLint level, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexLevelParameterfv(" << getTextureTargetStr(target) << ", " << level << ", " << getTextureLevelParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTexLevelParameterfv(target, level, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetTexLevelParameteriv (glw::GLenum target, glw::GLint level, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexLevelParameteriv(" << getTextureTargetStr(target) << ", " << level << ", " << getTextureLevelParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTexLevelParameteriv(target, level, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, 1) << TestLog::EndMessage;
}

void CallLogWrapper::glGetTexParameterIiv (glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameterIiv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTexParameterIiv(target, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetTexParameterIuiv (glw::GLenum target, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameterIuiv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTexParameterIuiv(target, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetTexParameterfv (glw::GLenum target, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameterfv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTexParameterfv(target, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetTexParameteriv (glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameteriv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTexParameteriv(target, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getTextureParamQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetTextureImage (glw::GLuint texture, glw::GLint level, glw::GLenum format, glw::GLenum type, glw::GLsizei bufSize, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureImage(" << texture << ", " << level << ", " << toHex(format) << ", " << toHex(type) << ", " << bufSize << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getTextureImage(texture, level, format, type, bufSize, pixels);
}

void CallLogWrapper::glGetTextureImageEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum format, glw::GLenum type, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureImageEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getTextureImageEXT(texture, target, level, format, type, pixels);
}

void CallLogWrapper::glGetTextureLevelParameterfv (glw::GLuint texture, glw::GLint level, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureLevelParameterfv(" << texture << ", " << level << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureLevelParameterfv(texture, level, pname, params);
}

void CallLogWrapper::glGetTextureLevelParameterfvEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureLevelParameterfvEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureLevelParameterfvEXT(texture, target, level, pname, params);
}

void CallLogWrapper::glGetTextureLevelParameteriv (glw::GLuint texture, glw::GLint level, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureLevelParameteriv(" << texture << ", " << level << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureLevelParameteriv(texture, level, pname, params);
}

void CallLogWrapper::glGetTextureLevelParameterivEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureLevelParameterivEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureLevelParameterivEXT(texture, target, level, pname, params);
}

void CallLogWrapper::glGetTextureParameterIiv (glw::GLuint texture, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterIiv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterIiv(texture, pname, params);
}

void CallLogWrapper::glGetTextureParameterIivEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterIivEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterIivEXT(texture, target, pname, params);
}

void CallLogWrapper::glGetTextureParameterIuiv (glw::GLuint texture, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterIuiv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterIuiv(texture, pname, params);
}

void CallLogWrapper::glGetTextureParameterIuivEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterIuivEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterIuivEXT(texture, target, pname, params);
}

void CallLogWrapper::glGetTextureParameterfv (glw::GLuint texture, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterfv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterfv(texture, pname, params);
}

void CallLogWrapper::glGetTextureParameterfvEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterfvEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterfvEXT(texture, target, pname, params);
}

void CallLogWrapper::glGetTextureParameteriv (glw::GLuint texture, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameteriv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameteriv(texture, pname, params);
}

void CallLogWrapper::glGetTextureParameterivEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureParameterivEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getTextureParameterivEXT(texture, target, pname, params);
}

void CallLogWrapper::glGetTextureSubImage (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, glw::GLsizei bufSize, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTextureSubImage(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << toHex(type) << ", " << bufSize << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, bufSize, pixels);
}

void CallLogWrapper::glGetTransformFeedbackVarying (glw::GLuint program, glw::GLuint index, glw::GLsizei bufSize, glw::GLsizei *length, glw::GLsizei *size, glw::GLenum *type, glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTransformFeedbackVarying(" << program << ", " << index << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(size))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(type))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(name))) << ");" << TestLog::EndMessage;
	m_gl.getTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void CallLogWrapper::glGetTransformFeedbacki64_v (glw::GLuint xfb, glw::GLenum pname, glw::GLuint index, glw::GLint64 *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTransformFeedbacki64_v(" << xfb << ", " << toHex(pname) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getTransformFeedbacki64_v(xfb, pname, index, param);
}

void CallLogWrapper::glGetTransformFeedbacki_v (glw::GLuint xfb, glw::GLenum pname, glw::GLuint index, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTransformFeedbacki_v(" << xfb << ", " << toHex(pname) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getTransformFeedbacki_v(xfb, pname, index, param);
}

void CallLogWrapper::glGetTransformFeedbackiv (glw::GLuint xfb, glw::GLenum pname, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTransformFeedbackiv(" << xfb << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getTransformFeedbackiv(xfb, pname, param);
}

glw::GLuint CallLogWrapper::glGetUniformBlockIndex (glw::GLuint program, const glw::GLchar *uniformBlockName)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformBlockIndex(" << program << ", " << getStringStr(uniformBlockName) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getUniformBlockIndex(program, uniformBlockName);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetUniformIndices (glw::GLuint program, glw::GLsizei uniformCount, const glw::GLchar *const*uniformNames, glw::GLuint *uniformIndices)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformIndices(" << program << ", " << uniformCount << ", " << getPointerStr(uniformNames, uniformCount) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(uniformIndices))) << ");" << TestLog::EndMessage;
	m_gl.getUniformIndices(program, uniformCount, uniformNames, uniformIndices);
	if (m_enableLog)
		m_log << TestLog::Message << "// uniformIndices = " << getPointerStr(uniformIndices, uniformCount) << TestLog::EndMessage;
}

glw::GLint CallLogWrapper::glGetUniformLocation (glw::GLuint program, const glw::GLchar *name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformLocation(" << program << ", " << getStringStr(name) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getUniformLocation(program, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetUniformSubroutineuiv (glw::GLenum shadertype, glw::GLint location, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformSubroutineuiv(" << toHex(shadertype) << ", " << location << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getUniformSubroutineuiv(shadertype, location, params);
}

void CallLogWrapper::glGetUniformdv (glw::GLuint program, glw::GLint location, glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformdv(" << program << ", " << location << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getUniformdv(program, location, params);
}

void CallLogWrapper::glGetUniformfv (glw::GLuint program, glw::GLint location, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformfv(" << program << ", " << location << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getUniformfv(program, location, params);
}

void CallLogWrapper::glGetUniformiv (glw::GLuint program, glw::GLint location, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformiv(" << program << ", " << location << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getUniformiv(program, location, params);
}

void CallLogWrapper::glGetUniformuiv (glw::GLuint program, glw::GLint location, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformuiv(" << program << ", " << location << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getUniformuiv(program, location, params);
}

void CallLogWrapper::glGetVertexArrayIndexed64iv (glw::GLuint vaobj, glw::GLuint index, glw::GLenum pname, glw::GLint64 *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayIndexed64iv(" << vaobj << ", " << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayIndexed64iv(vaobj, index, pname, param);
}

void CallLogWrapper::glGetVertexArrayIndexediv (glw::GLuint vaobj, glw::GLuint index, glw::GLenum pname, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayIndexediv(" << vaobj << ", " << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayIndexediv(vaobj, index, pname, param);
}

void CallLogWrapper::glGetVertexArrayIntegeri_vEXT (glw::GLuint vaobj, glw::GLuint index, glw::GLenum pname, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayIntegeri_vEXT(" << vaobj << ", " << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayIntegeri_vEXT(vaobj, index, pname, param);
}

void CallLogWrapper::glGetVertexArrayIntegervEXT (glw::GLuint vaobj, glw::GLenum pname, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayIntegervEXT(" << vaobj << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayIntegervEXT(vaobj, pname, param);
}

void CallLogWrapper::glGetVertexArrayPointeri_vEXT (glw::GLuint vaobj, glw::GLuint index, glw::GLenum pname, void **param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayPointeri_vEXT(" << vaobj << ", " << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayPointeri_vEXT(vaobj, index, pname, param);
}

void CallLogWrapper::glGetVertexArrayPointervEXT (glw::GLuint vaobj, glw::GLenum pname, void **param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayPointervEXT(" << vaobj << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayPointervEXT(vaobj, pname, param);
}

void CallLogWrapper::glGetVertexArrayiv (glw::GLuint vaobj, glw::GLenum pname, glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexArrayiv(" << vaobj << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.getVertexArrayiv(vaobj, pname, param);
}

void CallLogWrapper::glGetVertexAttribIiv (glw::GLuint index, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribIiv(" << index << ", " << getVertexAttribParameterNameStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribIiv(index, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getAttributeQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetVertexAttribIuiv (glw::GLuint index, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribIuiv(" << index << ", " << getVertexAttribParameterNameStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribIuiv(index, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getAttributeQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetVertexAttribLdv (glw::GLuint index, glw::GLenum pname, glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribLdv(" << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribLdv(index, pname, params);
}

void CallLogWrapper::glGetVertexAttribPointerv (glw::GLuint index, glw::GLenum pname, void **pointer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribPointerv(" << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(pointer))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribPointerv(index, pname, pointer);
}

void CallLogWrapper::glGetVertexAttribdv (glw::GLuint index, glw::GLenum pname, glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribdv(" << index << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribdv(index, pname, params);
}

void CallLogWrapper::glGetVertexAttribfv (glw::GLuint index, glw::GLenum pname, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribfv(" << index << ", " << getVertexAttribParameterNameStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribfv(index, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getAttributeQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetVertexAttribiv (glw::GLuint index, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribiv(" << index << ", " << getVertexAttribParameterNameStr(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribiv(index, pname, params);
	if (m_enableLog)
		m_log << TestLog::Message << "// params = " << getPointerStr(params, getAttributeQueryNumArgsOut(pname)) << TestLog::EndMessage;
}

void CallLogWrapper::glGetnCompressedTexImage (glw::GLenum target, glw::GLint lod, glw::GLsizei bufSize, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetnCompressedTexImage(" << toHex(target) << ", " << lod << ", " << bufSize << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getnCompressedTexImage(target, lod, bufSize, pixels);
}

void CallLogWrapper::glGetnTexImage (glw::GLenum target, glw::GLint level, glw::GLenum format, glw::GLenum type, glw::GLsizei bufSize, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetnTexImage(" << toHex(target) << ", " << level << ", " << toHex(format) << ", " << toHex(type) << ", " << bufSize << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.getnTexImage(target, level, format, type, bufSize, pixels);
}

void CallLogWrapper::glGetnUniformdv (glw::GLuint program, glw::GLint location, glw::GLsizei bufSize, glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetnUniformdv(" << program << ", " << location << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getnUniformdv(program, location, bufSize, params);
}

void CallLogWrapper::glGetnUniformfv (glw::GLuint program, glw::GLint location, glw::GLsizei bufSize, glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetnUniformfv(" << program << ", " << location << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getnUniformfv(program, location, bufSize, params);
}

void CallLogWrapper::glGetnUniformiv (glw::GLuint program, glw::GLint location, glw::GLsizei bufSize, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetnUniformiv(" << program << ", " << location << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getnUniformiv(program, location, bufSize, params);
}

void CallLogWrapper::glGetnUniformuiv (glw::GLuint program, glw::GLint location, glw::GLsizei bufSize, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetnUniformuiv(" << program << ", " << location << ", " << bufSize << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.getnUniformuiv(program, location, bufSize, params);
}

void CallLogWrapper::glHint (glw::GLenum target, glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glHint(" << getHintStr(target) << ", " << getHintModeStr(mode) << ");" << TestLog::EndMessage;
	m_gl.hint(target, mode);
}

void CallLogWrapper::glInsertEventMarkerEXT (glw::GLsizei length, const glw::GLchar *marker)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInsertEventMarkerEXT(" << length << ", " << getStringStr(marker) << ");" << TestLog::EndMessage;
	m_gl.insertEventMarkerEXT(length, marker);
}

void CallLogWrapper::glInvalidateBufferData (glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateBufferData(" << buffer << ");" << TestLog::EndMessage;
	m_gl.invalidateBufferData(buffer);
}

void CallLogWrapper::glInvalidateBufferSubData (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateBufferSubData(" << buffer << ", " << offset << ", " << length << ");" << TestLog::EndMessage;
	m_gl.invalidateBufferSubData(buffer, offset, length);
}

void CallLogWrapper::glInvalidateFramebuffer (glw::GLenum target, glw::GLsizei numAttachments, const glw::GLenum *attachments)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateFramebuffer(" << getFramebufferTargetStr(target) << ", " << numAttachments << ", " << getEnumPointerStr(attachments, numAttachments, getInvalidateAttachmentName) << ");" << TestLog::EndMessage;
	m_gl.invalidateFramebuffer(target, numAttachments, attachments);
}

void CallLogWrapper::glInvalidateNamedFramebufferData (glw::GLuint framebuffer, glw::GLsizei numAttachments, const glw::GLenum *attachments)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateNamedFramebufferData(" << framebuffer << ", " << numAttachments << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(attachments))) << ");" << TestLog::EndMessage;
	m_gl.invalidateNamedFramebufferData(framebuffer, numAttachments, attachments);
}

void CallLogWrapper::glInvalidateNamedFramebufferSubData (glw::GLuint framebuffer, glw::GLsizei numAttachments, const glw::GLenum *attachments, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateNamedFramebufferSubData(" << framebuffer << ", " << numAttachments << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(attachments))) << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.invalidateNamedFramebufferSubData(framebuffer, numAttachments, attachments, x, y, width, height);
}

void CallLogWrapper::glInvalidateSubFramebuffer (glw::GLenum target, glw::GLsizei numAttachments, const glw::GLenum *attachments, glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateSubFramebuffer(" << getFramebufferTargetStr(target) << ", " << numAttachments << ", " << getEnumPointerStr(attachments, numAttachments, getInvalidateAttachmentName) << ", " << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.invalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}

void CallLogWrapper::glInvalidateTexImage (glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateTexImage(" << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.invalidateTexImage(texture, level);
}

void CallLogWrapper::glInvalidateTexSubImage (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateTexSubImage(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ");" << TestLog::EndMessage;
	m_gl.invalidateTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth);
}

glw::GLboolean CallLogWrapper::glIsBuffer (glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsBuffer(" << buffer << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isBuffer(buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsEnabled (glw::GLenum cap)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsEnabled(" << getEnableCapStr(cap) << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isEnabled(cap);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsEnabledi (glw::GLenum target, glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsEnabledi(" << getIndexedEnableCapStr(target) << ", " << index << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isEnabledi(target, index);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsFramebuffer (glw::GLuint framebuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsFramebuffer(" << framebuffer << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isFramebuffer(framebuffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsProgram (glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsProgram(" << program << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isProgram(program);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsProgramPipeline (glw::GLuint pipeline)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsProgramPipeline(" << pipeline << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isProgramPipeline(pipeline);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsQuery (glw::GLuint id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsQuery(" << id << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isQuery(id);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsRenderbuffer (glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsRenderbuffer(" << renderbuffer << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isRenderbuffer(renderbuffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsSampler (glw::GLuint sampler)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsSampler(" << sampler << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isSampler(sampler);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsShader (glw::GLuint shader)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsShader(" << shader << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isShader(shader);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsSync (glw::GLsync sync)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsSync(" << sync << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isSync(sync);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsTexture (glw::GLuint texture)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsTexture(" << texture << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isTexture(texture);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsTransformFeedback (glw::GLuint id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsTransformFeedback(" << id << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isTransformFeedback(id);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsVertexArray (glw::GLuint array)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsVertexArray(" << array << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isVertexArray(array);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glLineWidth (glw::GLfloat width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glLineWidth(" << width << ");" << TestLog::EndMessage;
	m_gl.lineWidth(width);
}

void CallLogWrapper::glLinkProgram (glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glLinkProgram(" << program << ");" << TestLog::EndMessage;
	m_gl.linkProgram(program);
}

void CallLogWrapper::glLogicOp (glw::GLenum opcode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glLogicOp(" << toHex(opcode) << ");" << TestLog::EndMessage;
	m_gl.logicOp(opcode);
}

void * CallLogWrapper::glMapBuffer (glw::GLenum target, glw::GLenum access)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapBuffer(" << toHex(target) << ", " << toHex(access) << ");" << TestLog::EndMessage;
	void * returnValue = m_gl.mapBuffer(target, access);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void * CallLogWrapper::glMapBufferRange (glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr length, glw::GLbitfield access)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapBufferRange(" << getBufferTargetStr(target) << ", " << offset << ", " << length << ", " << getBufferMapFlagsStr(access) << ");" << TestLog::EndMessage;
	void * returnValue = m_gl.mapBufferRange(target, offset, length, access);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void * CallLogWrapper::glMapNamedBuffer (glw::GLuint buffer, glw::GLenum access)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapNamedBuffer(" << buffer << ", " << toHex(access) << ");" << TestLog::EndMessage;
	void * returnValue = m_gl.mapNamedBuffer(buffer, access);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void * CallLogWrapper::glMapNamedBufferEXT (glw::GLuint buffer, glw::GLenum access)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapNamedBufferEXT(" << buffer << ", " << toHex(access) << ");" << TestLog::EndMessage;
	void * returnValue = m_gl.mapNamedBufferEXT(buffer, access);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void * CallLogWrapper::glMapNamedBufferRange (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr length, glw::GLbitfield access)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapNamedBufferRange(" << buffer << ", " << offset << ", " << length << ", " << toHex(access) << ");" << TestLog::EndMessage;
	void * returnValue = m_gl.mapNamedBufferRange(buffer, offset, length, access);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void * CallLogWrapper::glMapNamedBufferRangeEXT (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr length, glw::GLbitfield access)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapNamedBufferRangeEXT(" << buffer << ", " << offset << ", " << length << ", " << toHex(access) << ");" << TestLog::EndMessage;
	void * returnValue = m_gl.mapNamedBufferRangeEXT(buffer, offset, length, access);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glMatrixFrustumEXT (glw::GLenum mode, glw::GLdouble left, glw::GLdouble right, glw::GLdouble bottom, glw::GLdouble top, glw::GLdouble zNear, glw::GLdouble zFar)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixFrustumEXT(" << toHex(mode) << ", " << left << ", " << right << ", " << bottom << ", " << top << ", " << zNear << ", " << zFar << ");" << TestLog::EndMessage;
	m_gl.matrixFrustumEXT(mode, left, right, bottom, top, zNear, zFar);
}

void CallLogWrapper::glMatrixLoadIdentityEXT (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixLoadIdentityEXT(" << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.matrixLoadIdentityEXT(mode);
}

void CallLogWrapper::glMatrixLoadTransposedEXT (glw::GLenum mode, const glw::GLdouble *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixLoadTransposedEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixLoadTransposedEXT(mode, m);
}

void CallLogWrapper::glMatrixLoadTransposefEXT (glw::GLenum mode, const glw::GLfloat *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixLoadTransposefEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixLoadTransposefEXT(mode, m);
}

void CallLogWrapper::glMatrixLoaddEXT (glw::GLenum mode, const glw::GLdouble *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixLoaddEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixLoaddEXT(mode, m);
}

void CallLogWrapper::glMatrixLoadfEXT (glw::GLenum mode, const glw::GLfloat *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixLoadfEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixLoadfEXT(mode, m);
}

void CallLogWrapper::glMatrixMultTransposedEXT (glw::GLenum mode, const glw::GLdouble *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixMultTransposedEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixMultTransposedEXT(mode, m);
}

void CallLogWrapper::glMatrixMultTransposefEXT (glw::GLenum mode, const glw::GLfloat *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixMultTransposefEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixMultTransposefEXT(mode, m);
}

void CallLogWrapper::glMatrixMultdEXT (glw::GLenum mode, const glw::GLdouble *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixMultdEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixMultdEXT(mode, m);
}

void CallLogWrapper::glMatrixMultfEXT (glw::GLenum mode, const glw::GLfloat *m)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixMultfEXT(" << toHex(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(m))) << ");" << TestLog::EndMessage;
	m_gl.matrixMultfEXT(mode, m);
}

void CallLogWrapper::glMatrixOrthoEXT (glw::GLenum mode, glw::GLdouble left, glw::GLdouble right, glw::GLdouble bottom, glw::GLdouble top, glw::GLdouble zNear, glw::GLdouble zFar)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixOrthoEXT(" << toHex(mode) << ", " << left << ", " << right << ", " << bottom << ", " << top << ", " << zNear << ", " << zFar << ");" << TestLog::EndMessage;
	m_gl.matrixOrthoEXT(mode, left, right, bottom, top, zNear, zFar);
}

void CallLogWrapper::glMatrixPopEXT (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixPopEXT(" << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.matrixPopEXT(mode);
}

void CallLogWrapper::glMatrixPushEXT (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixPushEXT(" << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.matrixPushEXT(mode);
}

void CallLogWrapper::glMatrixRotatedEXT (glw::GLenum mode, glw::GLdouble angle, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixRotatedEXT(" << toHex(mode) << ", " << angle << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.matrixRotatedEXT(mode, angle, x, y, z);
}

void CallLogWrapper::glMatrixRotatefEXT (glw::GLenum mode, glw::GLfloat angle, glw::GLfloat x, glw::GLfloat y, glw::GLfloat z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixRotatefEXT(" << toHex(mode) << ", " << angle << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.matrixRotatefEXT(mode, angle, x, y, z);
}

void CallLogWrapper::glMatrixScaledEXT (glw::GLenum mode, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixScaledEXT(" << toHex(mode) << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.matrixScaledEXT(mode, x, y, z);
}

void CallLogWrapper::glMatrixScalefEXT (glw::GLenum mode, glw::GLfloat x, glw::GLfloat y, glw::GLfloat z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixScalefEXT(" << toHex(mode) << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.matrixScalefEXT(mode, x, y, z);
}

void CallLogWrapper::glMatrixTranslatedEXT (glw::GLenum mode, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixTranslatedEXT(" << toHex(mode) << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.matrixTranslatedEXT(mode, x, y, z);
}

void CallLogWrapper::glMatrixTranslatefEXT (glw::GLenum mode, glw::GLfloat x, glw::GLfloat y, glw::GLfloat z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMatrixTranslatefEXT(" << toHex(mode) << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.matrixTranslatefEXT(mode, x, y, z);
}

void CallLogWrapper::glMaxShaderCompilerThreadsKHR (glw::GLuint count)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMaxShaderCompilerThreadsKHR(" << count << ");" << TestLog::EndMessage;
	m_gl.maxShaderCompilerThreadsKHR(count);
}

void CallLogWrapper::glMemoryBarrier (glw::GLbitfield barriers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMemoryBarrier(" << getMemoryBarrierFlagsStr(barriers) << ");" << TestLog::EndMessage;
	m_gl.memoryBarrier(barriers);
}

void CallLogWrapper::glMemoryBarrierByRegion (glw::GLbitfield barriers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMemoryBarrierByRegion(" << toHex(barriers) << ");" << TestLog::EndMessage;
	m_gl.memoryBarrierByRegion(barriers);
}

void CallLogWrapper::glMinSampleShading (glw::GLfloat value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMinSampleShading(" << value << ");" << TestLog::EndMessage;
	m_gl.minSampleShading(value);
}

void CallLogWrapper::glMultiDrawArrays (glw::GLenum mode, const glw::GLint *first, const glw::GLsizei *count, glw::GLsizei drawcount)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawArrays(" << getPrimitiveTypeStr(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(first))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(count))) << ", " << drawcount << ");" << TestLog::EndMessage;
	m_gl.multiDrawArrays(mode, first, count, drawcount);
}

void CallLogWrapper::glMultiDrawArraysIndirect (glw::GLenum mode, const void *indirect, glw::GLsizei drawcount, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawArraysIndirect(" << toHex(mode) << ", " << indirect << ", " << drawcount << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.multiDrawArraysIndirect(mode, indirect, drawcount, stride);
}

void CallLogWrapper::glMultiDrawArraysIndirectCount (glw::GLenum mode, const void *indirect, glw::GLintptr drawcount, glw::GLsizei maxdrawcount, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawArraysIndirectCount(" << toHex(mode) << ", " << indirect << ", " << drawcount << ", " << maxdrawcount << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.multiDrawArraysIndirectCount(mode, indirect, drawcount, maxdrawcount, stride);
}

void CallLogWrapper::glMultiDrawElements (glw::GLenum mode, const glw::GLsizei *count, glw::GLenum type, const void *const*indices, glw::GLsizei drawcount)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElements(" << getPrimitiveTypeStr(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(count))) << ", " << getTypeStr(type) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(indices))) << ", " << drawcount << ");" << TestLog::EndMessage;
	m_gl.multiDrawElements(mode, count, type, indices, drawcount);
}

void CallLogWrapper::glMultiDrawElementsBaseVertex (glw::GLenum mode, const glw::GLsizei *count, glw::GLenum type, const void *const*indices, glw::GLsizei drawcount, const glw::GLint *basevertex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElementsBaseVertex(" << getPrimitiveTypeStr(mode) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(count))) << ", " << getTypeStr(type) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(indices))) << ", " << drawcount << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(basevertex))) << ");" << TestLog::EndMessage;
	m_gl.multiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
}

void CallLogWrapper::glMultiDrawElementsIndirect (glw::GLenum mode, glw::GLenum type, const void *indirect, glw::GLsizei drawcount, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElementsIndirect(" << toHex(mode) << ", " << toHex(type) << ", " << indirect << ", " << drawcount << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.multiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
}

void CallLogWrapper::glMultiDrawElementsIndirectCount (glw::GLenum mode, glw::GLenum type, const void *indirect, glw::GLintptr drawcount, glw::GLsizei maxdrawcount, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElementsIndirectCount(" << toHex(mode) << ", " << toHex(type) << ", " << indirect << ", " << drawcount << ", " << maxdrawcount << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.multiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
}

void CallLogWrapper::glMultiTexBufferEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum internalformat, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexBufferEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(internalformat) << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.multiTexBufferEXT(texunit, target, internalformat, buffer);
}

void CallLogWrapper::glMultiTexCoordPointerEXT (glw::GLenum texunit, glw::GLint size, glw::GLenum type, glw::GLsizei stride, const void *pointer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexCoordPointerEXT(" << toHex(texunit) << ", " << size << ", " << toHex(type) << ", " << stride << ", " << pointer << ");" << TestLog::EndMessage;
	m_gl.multiTexCoordPointerEXT(texunit, size, type, stride, pointer);
}

void CallLogWrapper::glMultiTexEnvfEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexEnvfEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexEnvfEXT(texunit, target, pname, param);
}

void CallLogWrapper::glMultiTexEnvfvEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexEnvfvEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexEnvfvEXT(texunit, target, pname, params);
}

void CallLogWrapper::glMultiTexEnviEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexEnviEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexEnviEXT(texunit, target, pname, param);
}

void CallLogWrapper::glMultiTexEnvivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexEnvivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexEnvivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glMultiTexGendEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, glw::GLdouble param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexGendEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexGendEXT(texunit, coord, pname, param);
}

void CallLogWrapper::glMultiTexGendvEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, const glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexGendvEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexGendvEXT(texunit, coord, pname, params);
}

void CallLogWrapper::glMultiTexGenfEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexGenfEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexGenfEXT(texunit, coord, pname, param);
}

void CallLogWrapper::glMultiTexGenfvEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexGenfvEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexGenfvEXT(texunit, coord, pname, params);
}

void CallLogWrapper::glMultiTexGeniEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexGeniEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexGeniEXT(texunit, coord, pname, param);
}

void CallLogWrapper::glMultiTexGenivEXT (glw::GLenum texunit, glw::GLenum coord, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexGenivEXT(" << toHex(texunit) << ", " << toHex(coord) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexGenivEXT(texunit, coord, pname, params);
}

void CallLogWrapper::glMultiTexImage1DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexImage1DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << internalformat << ", " << width << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.multiTexImage1DEXT(texunit, target, level, internalformat, width, border, format, type, pixels);
}

void CallLogWrapper::glMultiTexImage2DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexImage2DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << internalformat << ", " << width << ", " << height << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.multiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, format, type, pixels);
}

void CallLogWrapper::glMultiTexImage3DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexImage3DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << internalformat << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.multiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void CallLogWrapper::glMultiTexParameterIivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexParameterIivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexParameterIivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glMultiTexParameterIuivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, const glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexParameterIuivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexParameterIuivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glMultiTexParameterfEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexParameterfEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexParameterfEXT(texunit, target, pname, param);
}

void CallLogWrapper::glMultiTexParameterfvEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexParameterfvEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexParameterfvEXT(texunit, target, pname, params);
}

void CallLogWrapper::glMultiTexParameteriEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexParameteriEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.multiTexParameteriEXT(texunit, target, pname, param);
}

void CallLogWrapper::glMultiTexParameterivEXT (glw::GLenum texunit, glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexParameterivEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multiTexParameterivEXT(texunit, target, pname, params);
}

void CallLogWrapper::glMultiTexRenderbufferEXT (glw::GLenum texunit, glw::GLenum target, glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexRenderbufferEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << renderbuffer << ");" << TestLog::EndMessage;
	m_gl.multiTexRenderbufferEXT(texunit, target, renderbuffer);
}

void CallLogWrapper::glMultiTexSubImage1DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexSubImage1DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.multiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, type, pixels);
}

void CallLogWrapper::glMultiTexSubImage2DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexSubImage2DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.multiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void CallLogWrapper::glMultiTexSubImage3DEXT (glw::GLenum texunit, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiTexSubImage3DEXT(" << toHex(texunit) << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.multiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void CallLogWrapper::glMulticastBarrierNV (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastBarrierNV(" << ");" << TestLog::EndMessage;
	m_gl.multicastBarrierNV();
}

void CallLogWrapper::glMulticastBlitFramebufferNV (glw::GLuint srcGpu, glw::GLuint dstGpu, glw::GLint srcX0, glw::GLint srcY0, glw::GLint srcX1, glw::GLint srcY1, glw::GLint dstX0, glw::GLint dstY0, glw::GLint dstX1, glw::GLint dstY1, glw::GLbitfield mask, glw::GLenum filter)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastBlitFramebufferNV(" << srcGpu << ", " << dstGpu << ", " << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", " << dstY1 << ", " << toHex(mask) << ", " << toHex(filter) << ");" << TestLog::EndMessage;
	m_gl.multicastBlitFramebufferNV(srcGpu, dstGpu, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void CallLogWrapper::glMulticastBufferSubDataNV (glw::GLbitfield gpuMask, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastBufferSubDataNV(" << toHex(gpuMask) << ", " << buffer << ", " << offset << ", " << size << ", " << data << ");" << TestLog::EndMessage;
	m_gl.multicastBufferSubDataNV(gpuMask, buffer, offset, size, data);
}

void CallLogWrapper::glMulticastCopyBufferSubDataNV (glw::GLuint readGpu, glw::GLbitfield writeGpuMask, glw::GLuint readBuffer, glw::GLuint writeBuffer, glw::GLintptr readOffset, glw::GLintptr writeOffset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastCopyBufferSubDataNV(" << readGpu << ", " << toHex(writeGpuMask) << ", " << readBuffer << ", " << writeBuffer << ", " << readOffset << ", " << writeOffset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.multicastCopyBufferSubDataNV(readGpu, writeGpuMask, readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void CallLogWrapper::glMulticastCopyImageSubDataNV (glw::GLuint srcGpu, glw::GLbitfield dstGpuMask, glw::GLuint srcName, glw::GLenum srcTarget, glw::GLint srcLevel, glw::GLint srcX, glw::GLint srcY, glw::GLint srcZ, glw::GLuint dstName, glw::GLenum dstTarget, glw::GLint dstLevel, glw::GLint dstX, glw::GLint dstY, glw::GLint dstZ, glw::GLsizei srcWidth, glw::GLsizei srcHeight, glw::GLsizei srcDepth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastCopyImageSubDataNV(" << srcGpu << ", " << toHex(dstGpuMask) << ", " << srcName << ", " << toHex(srcTarget) << ", " << srcLevel << ", " << srcX << ", " << srcY << ", " << srcZ << ", " << dstName << ", " << toHex(dstTarget) << ", " << dstLevel << ", " << dstX << ", " << dstY << ", " << dstZ << ", " << srcWidth << ", " << srcHeight << ", " << srcDepth << ");" << TestLog::EndMessage;
	m_gl.multicastCopyImageSubDataNV(srcGpu, dstGpuMask, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void CallLogWrapper::glMulticastFramebufferSampleLocationsfvNV (glw::GLuint gpu, glw::GLuint framebuffer, glw::GLuint start, glw::GLsizei count, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastFramebufferSampleLocationsfvNV(" << gpu << ", " << framebuffer << ", " << start << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.multicastFramebufferSampleLocationsfvNV(gpu, framebuffer, start, count, v);
}

void CallLogWrapper::glMulticastGetQueryObjecti64vNV (glw::GLuint gpu, glw::GLuint id, glw::GLenum pname, glw::GLint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastGetQueryObjecti64vNV(" << gpu << ", " << id << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multicastGetQueryObjecti64vNV(gpu, id, pname, params);
}

void CallLogWrapper::glMulticastGetQueryObjectivNV (glw::GLuint gpu, glw::GLuint id, glw::GLenum pname, glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastGetQueryObjectivNV(" << gpu << ", " << id << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multicastGetQueryObjectivNV(gpu, id, pname, params);
}

void CallLogWrapper::glMulticastGetQueryObjectui64vNV (glw::GLuint gpu, glw::GLuint id, glw::GLenum pname, glw::GLuint64 *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastGetQueryObjectui64vNV(" << gpu << ", " << id << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multicastGetQueryObjectui64vNV(gpu, id, pname, params);
}

void CallLogWrapper::glMulticastGetQueryObjectuivNV (glw::GLuint gpu, glw::GLuint id, glw::GLenum pname, glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastGetQueryObjectuivNV(" << gpu << ", " << id << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.multicastGetQueryObjectuivNV(gpu, id, pname, params);
}

void CallLogWrapper::glMulticastWaitSyncNV (glw::GLuint signalGpu, glw::GLbitfield waitGpuMask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMulticastWaitSyncNV(" << signalGpu << ", " << toHex(waitGpuMask) << ");" << TestLog::EndMessage;
	m_gl.multicastWaitSyncNV(signalGpu, waitGpuMask);
}

void CallLogWrapper::glNamedBufferData (glw::GLuint buffer, glw::GLsizeiptr size, const void *data, glw::GLenum usage)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedBufferData(" << buffer << ", " << size << ", " << data << ", " << toHex(usage) << ");" << TestLog::EndMessage;
	m_gl.namedBufferData(buffer, size, data, usage);
}

void CallLogWrapper::glNamedBufferDataEXT (glw::GLuint buffer, glw::GLsizeiptr size, const void *data, glw::GLenum usage)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedBufferDataEXT(" << buffer << ", " << size << ", " << data << ", " << toHex(usage) << ");" << TestLog::EndMessage;
	m_gl.namedBufferDataEXT(buffer, size, data, usage);
}

void CallLogWrapper::glNamedBufferPageCommitmentARB (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size, glw::GLboolean commit)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedBufferPageCommitmentARB(" << buffer << ", " << offset << ", " << size << ", " << getBooleanStr(commit) << ");" << TestLog::EndMessage;
	m_gl.namedBufferPageCommitmentARB(buffer, offset, size, commit);
}

void CallLogWrapper::glNamedBufferPageCommitmentEXT (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size, glw::GLboolean commit)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedBufferPageCommitmentEXT(" << buffer << ", " << offset << ", " << size << ", " << getBooleanStr(commit) << ");" << TestLog::EndMessage;
	m_gl.namedBufferPageCommitmentEXT(buffer, offset, size, commit);
}

void CallLogWrapper::glNamedBufferStorage (glw::GLuint buffer, glw::GLsizeiptr size, const void *data, glw::GLbitfield flags)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedBufferStorage(" << buffer << ", " << size << ", " << data << ", " << toHex(flags) << ");" << TestLog::EndMessage;
	m_gl.namedBufferStorage(buffer, size, data, flags);
}

void CallLogWrapper::glNamedBufferSubData (glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size, const void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedBufferSubData(" << buffer << ", " << offset << ", " << size << ", " << data << ");" << TestLog::EndMessage;
	m_gl.namedBufferSubData(buffer, offset, size, data);
}

void CallLogWrapper::glNamedCopyBufferSubDataEXT (glw::GLuint readBuffer, glw::GLuint writeBuffer, glw::GLintptr readOffset, glw::GLintptr writeOffset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedCopyBufferSubDataEXT(" << readBuffer << ", " << writeBuffer << ", " << readOffset << ", " << writeOffset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.namedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void CallLogWrapper::glNamedFramebufferDrawBuffer (glw::GLuint framebuffer, glw::GLenum buf)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferDrawBuffer(" << framebuffer << ", " << toHex(buf) << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferDrawBuffer(framebuffer, buf);
}

void CallLogWrapper::glNamedFramebufferDrawBuffers (glw::GLuint framebuffer, glw::GLsizei n, const glw::GLenum *bufs)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferDrawBuffers(" << framebuffer << ", " << n << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(bufs))) << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferDrawBuffers(framebuffer, n, bufs);
}

void CallLogWrapper::glNamedFramebufferParameteri (glw::GLuint framebuffer, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferParameteri(" << framebuffer << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferParameteri(framebuffer, pname, param);
}

void CallLogWrapper::glNamedFramebufferParameteriEXT (glw::GLuint framebuffer, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferParameteriEXT(" << framebuffer << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferParameteriEXT(framebuffer, pname, param);
}

void CallLogWrapper::glNamedFramebufferReadBuffer (glw::GLuint framebuffer, glw::GLenum src)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferReadBuffer(" << framebuffer << ", " << toHex(src) << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferReadBuffer(framebuffer, src);
}

void CallLogWrapper::glNamedFramebufferRenderbuffer (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum renderbuffertarget, glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferRenderbuffer(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(renderbuffertarget) << ", " << renderbuffer << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferRenderbuffer(framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void CallLogWrapper::glNamedFramebufferRenderbufferEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum renderbuffertarget, glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferRenderbufferEXT(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(renderbuffertarget) << ", " << renderbuffer << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void CallLogWrapper::glNamedFramebufferTexture (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTexture(" << framebuffer << ", " << toHex(attachment) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTexture(framebuffer, attachment, texture, level);
}

void CallLogWrapper::glNamedFramebufferTexture1DEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTexture1DEXT(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);
}

void CallLogWrapper::glNamedFramebufferTexture2DEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTexture2DEXT(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
}

void CallLogWrapper::glNamedFramebufferTexture3DEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLenum textarget, glw::GLuint texture, glw::GLint level, glw::GLint zoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTexture3DEXT(" << framebuffer << ", " << toHex(attachment) << ", " << toHex(textarget) << ", " << texture << ", " << level << ", " << zoffset << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level, zoffset);
}

void CallLogWrapper::glNamedFramebufferTextureEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLuint texture, glw::GLint level)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTextureEXT(" << framebuffer << ", " << toHex(attachment) << ", " << texture << ", " << level << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTextureEXT(framebuffer, attachment, texture, level);
}

void CallLogWrapper::glNamedFramebufferTextureFaceEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLenum face)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTextureFaceEXT(" << framebuffer << ", " << toHex(attachment) << ", " << texture << ", " << level << ", " << toHex(face) << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTextureFaceEXT(framebuffer, attachment, texture, level, face);
}

void CallLogWrapper::glNamedFramebufferTextureLayer (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLint layer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTextureLayer(" << framebuffer << ", " << toHex(attachment) << ", " << texture << ", " << level << ", " << layer << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTextureLayer(framebuffer, attachment, texture, level, layer);
}

void CallLogWrapper::glNamedFramebufferTextureLayerEXT (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLint layer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTextureLayerEXT(" << framebuffer << ", " << toHex(attachment) << ", " << texture << ", " << level << ", " << layer << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
}

void CallLogWrapper::glNamedFramebufferTextureMultiviewOVR (glw::GLuint framebuffer, glw::GLenum attachment, glw::GLuint texture, glw::GLint level, glw::GLint baseViewIndex, glw::GLsizei numViews)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedFramebufferTextureMultiviewOVR(" << framebuffer << ", " << toHex(attachment) << ", " << texture << ", " << level << ", " << baseViewIndex << ", " << numViews << ");" << TestLog::EndMessage;
	m_gl.namedFramebufferTextureMultiviewOVR(framebuffer, attachment, texture, level, baseViewIndex, numViews);
}

void CallLogWrapper::glNamedProgramLocalParameter4dEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z, glw::GLdouble w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameter4dEXT(" << program << ", " << toHex(target) << ", " << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameter4dEXT(program, target, index, x, y, z, w);
}

void CallLogWrapper::glNamedProgramLocalParameter4dvEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, const glw::GLdouble *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameter4dvEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameter4dvEXT(program, target, index, params);
}

void CallLogWrapper::glNamedProgramLocalParameter4fEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLfloat x, glw::GLfloat y, glw::GLfloat z, glw::GLfloat w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameter4fEXT(" << program << ", " << toHex(target) << ", " << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameter4fEXT(program, target, index, x, y, z, w);
}

void CallLogWrapper::glNamedProgramLocalParameter4fvEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameter4fvEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameter4fvEXT(program, target, index, params);
}

void CallLogWrapper::glNamedProgramLocalParameterI4iEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLint x, glw::GLint y, glw::GLint z, glw::GLint w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameterI4iEXT(" << program << ", " << toHex(target) << ", " << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameterI4iEXT(program, target, index, x, y, z, w);
}

void CallLogWrapper::glNamedProgramLocalParameterI4ivEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameterI4ivEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameterI4ivEXT(program, target, index, params);
}

void CallLogWrapper::glNamedProgramLocalParameterI4uiEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLuint x, glw::GLuint y, glw::GLuint z, glw::GLuint w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameterI4uiEXT(" << program << ", " << toHex(target) << ", " << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameterI4uiEXT(program, target, index, x, y, z, w);
}

void CallLogWrapper::glNamedProgramLocalParameterI4uivEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, const glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameterI4uivEXT(" << program << ", " << toHex(target) << ", " << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameterI4uivEXT(program, target, index, params);
}

void CallLogWrapper::glNamedProgramLocalParameters4fvEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLsizei count, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParameters4fvEXT(" << program << ", " << toHex(target) << ", " << index << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParameters4fvEXT(program, target, index, count, params);
}

void CallLogWrapper::glNamedProgramLocalParametersI4ivEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLsizei count, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParametersI4ivEXT(" << program << ", " << toHex(target) << ", " << index << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParametersI4ivEXT(program, target, index, count, params);
}

void CallLogWrapper::glNamedProgramLocalParametersI4uivEXT (glw::GLuint program, glw::GLenum target, glw::GLuint index, glw::GLsizei count, const glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramLocalParametersI4uivEXT(" << program << ", " << toHex(target) << ", " << index << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.namedProgramLocalParametersI4uivEXT(program, target, index, count, params);
}

void CallLogWrapper::glNamedProgramStringEXT (glw::GLuint program, glw::GLenum target, glw::GLenum format, glw::GLsizei len, const void *string)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedProgramStringEXT(" << program << ", " << toHex(target) << ", " << toHex(format) << ", " << len << ", " << string << ");" << TestLog::EndMessage;
	m_gl.namedProgramStringEXT(program, target, format, len, string);
}

void CallLogWrapper::glNamedRenderbufferStorage (glw::GLuint renderbuffer, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedRenderbufferStorage(" << renderbuffer << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.namedRenderbufferStorage(renderbuffer, internalformat, width, height);
}

void CallLogWrapper::glNamedRenderbufferStorageEXT (glw::GLuint renderbuffer, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedRenderbufferStorageEXT(" << renderbuffer << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.namedRenderbufferStorageEXT(renderbuffer, internalformat, width, height);
}

void CallLogWrapper::glNamedRenderbufferStorageMultisample (glw::GLuint renderbuffer, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedRenderbufferStorageMultisample(" << renderbuffer << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.namedRenderbufferStorageMultisample(renderbuffer, samples, internalformat, width, height);
}

void CallLogWrapper::glNamedRenderbufferStorageMultisampleCoverageEXT (glw::GLuint renderbuffer, glw::GLsizei coverageSamples, glw::GLsizei colorSamples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedRenderbufferStorageMultisampleCoverageEXT(" << renderbuffer << ", " << coverageSamples << ", " << colorSamples << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.namedRenderbufferStorageMultisampleCoverageEXT(renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
}

void CallLogWrapper::glNamedRenderbufferStorageMultisampleEXT (glw::GLuint renderbuffer, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glNamedRenderbufferStorageMultisampleEXT(" << renderbuffer << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.namedRenderbufferStorageMultisampleEXT(renderbuffer, samples, internalformat, width, height);
}

void CallLogWrapper::glObjectLabel (glw::GLenum identifier, glw::GLuint name, glw::GLsizei length, const glw::GLchar *label)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glObjectLabel(" << toHex(identifier) << ", " << name << ", " << length << ", " << getStringStr(label) << ");" << TestLog::EndMessage;
	m_gl.objectLabel(identifier, name, length, label);
}

void CallLogWrapper::glObjectPtrLabel (const void *ptr, glw::GLsizei length, const glw::GLchar *label)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glObjectPtrLabel(" << ptr << ", " << length << ", " << getStringStr(label) << ");" << TestLog::EndMessage;
	m_gl.objectPtrLabel(ptr, length, label);
}

void CallLogWrapper::glPatchParameterfv (glw::GLenum pname, const glw::GLfloat *values)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPatchParameterfv(" << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(values))) << ");" << TestLog::EndMessage;
	m_gl.patchParameterfv(pname, values);
}

void CallLogWrapper::glPatchParameteri (glw::GLenum pname, glw::GLint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPatchParameteri(" << getPatchParamStr(pname) << ", " << value << ");" << TestLog::EndMessage;
	m_gl.patchParameteri(pname, value);
}

void CallLogWrapper::glPauseTransformFeedback (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPauseTransformFeedback(" << ");" << TestLog::EndMessage;
	m_gl.pauseTransformFeedback();
}

void CallLogWrapper::glPixelStoref (glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPixelStoref(" << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.pixelStoref(pname, param);
}

void CallLogWrapper::glPixelStorei (glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPixelStorei(" << getPixelStoreParameterStr(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.pixelStorei(pname, param);
}

void CallLogWrapper::glPointParameterf (glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameterf(" << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.pointParameterf(pname, param);
}

void CallLogWrapper::glPointParameterfv (glw::GLenum pname, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameterfv(" << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.pointParameterfv(pname, params);
}

void CallLogWrapper::glPointParameteri (glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameteri(" << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.pointParameteri(pname, param);
}

void CallLogWrapper::glPointParameteriv (glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameteriv(" << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.pointParameteriv(pname, params);
}

void CallLogWrapper::glPointSize (glw::GLfloat size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointSize(" << size << ");" << TestLog::EndMessage;
	m_gl.pointSize(size);
}

void CallLogWrapper::glPolygonMode (glw::GLenum face, glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPolygonMode(" << toHex(face) << ", " << toHex(mode) << ");" << TestLog::EndMessage;
	m_gl.polygonMode(face, mode);
}

void CallLogWrapper::glPolygonOffset (glw::GLfloat factor, glw::GLfloat units)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPolygonOffset(" << factor << ", " << units << ");" << TestLog::EndMessage;
	m_gl.polygonOffset(factor, units);
}

void CallLogWrapper::glPolygonOffsetClamp (glw::GLfloat factor, glw::GLfloat units, glw::GLfloat clamp)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPolygonOffsetClamp(" << factor << ", " << units << ", " << clamp << ");" << TestLog::EndMessage;
	m_gl.polygonOffsetClamp(factor, units, clamp);
}

void CallLogWrapper::glPopDebugGroup (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPopDebugGroup(" << ");" << TestLog::EndMessage;
	m_gl.popDebugGroup();
}

void CallLogWrapper::glPopGroupMarkerEXT (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPopGroupMarkerEXT(" << ");" << TestLog::EndMessage;
	m_gl.popGroupMarkerEXT();
}

void CallLogWrapper::glPrimitiveBoundingBox (glw::GLfloat minX, glw::GLfloat minY, glw::GLfloat minZ, glw::GLfloat minW, glw::GLfloat maxX, glw::GLfloat maxY, glw::GLfloat maxZ, glw::GLfloat maxW)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPrimitiveBoundingBox(" << minX << ", " << minY << ", " << minZ << ", " << minW << ", " << maxX << ", " << maxY << ", " << maxZ << ", " << maxW << ");" << TestLog::EndMessage;
	m_gl.primitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
}

void CallLogWrapper::glPrimitiveRestartIndex (glw::GLuint index)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPrimitiveRestartIndex(" << index << ");" << TestLog::EndMessage;
	m_gl.primitiveRestartIndex(index);
}

void CallLogWrapper::glProgramBinary (glw::GLuint program, glw::GLenum binaryFormat, const void *binary, glw::GLsizei length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramBinary(" << program << ", " << toHex(binaryFormat) << ", " << binary << ", " << length << ");" << TestLog::EndMessage;
	m_gl.programBinary(program, binaryFormat, binary, length);
}

void CallLogWrapper::glProgramParameteri (glw::GLuint program, glw::GLenum pname, glw::GLint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramParameteri(" << program << ", " << getProgramParamStr(pname) << ", " << value << ");" << TestLog::EndMessage;
	m_gl.programParameteri(program, pname, value);
}

void CallLogWrapper::glProgramUniform1d (glw::GLuint program, glw::GLint location, glw::GLdouble v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1d(" << program << ", " << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.programUniform1d(program, location, v0);
}

void CallLogWrapper::glProgramUniform1dEXT (glw::GLuint program, glw::GLint location, glw::GLdouble x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1dEXT(" << program << ", " << location << ", " << x << ");" << TestLog::EndMessage;
	m_gl.programUniform1dEXT(program, location, x);
}

void CallLogWrapper::glProgramUniform1dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1dv(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform1dv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform1dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1dvEXT(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform1dvEXT(program, location, count, value);
}

void CallLogWrapper::glProgramUniform1f (glw::GLuint program, glw::GLint location, glw::GLfloat v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1f(" << program << ", " << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.programUniform1f(program, location, v0);
}

void CallLogWrapper::glProgramUniform1fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1fv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 1)) << ");" << TestLog::EndMessage;
	m_gl.programUniform1fv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform1i (glw::GLuint program, glw::GLint location, glw::GLint v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1i(" << program << ", " << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.programUniform1i(program, location, v0);
}

void CallLogWrapper::glProgramUniform1iv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1iv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 1)) << ");" << TestLog::EndMessage;
	m_gl.programUniform1iv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform1ui (glw::GLuint program, glw::GLint location, glw::GLuint v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1ui(" << program << ", " << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.programUniform1ui(program, location, v0);
}

void CallLogWrapper::glProgramUniform1uiv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1uiv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 1)) << ");" << TestLog::EndMessage;
	m_gl.programUniform1uiv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform2d (glw::GLuint program, glw::GLint location, glw::GLdouble v0, glw::GLdouble v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2d(" << program << ", " << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.programUniform2d(program, location, v0, v1);
}

void CallLogWrapper::glProgramUniform2dEXT (glw::GLuint program, glw::GLint location, glw::GLdouble x, glw::GLdouble y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2dEXT(" << program << ", " << location << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.programUniform2dEXT(program, location, x, y);
}

void CallLogWrapper::glProgramUniform2dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2dv(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform2dv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform2dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2dvEXT(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform2dvEXT(program, location, count, value);
}

void CallLogWrapper::glProgramUniform2f (glw::GLuint program, glw::GLint location, glw::GLfloat v0, glw::GLfloat v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2f(" << program << ", " << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.programUniform2f(program, location, v0, v1);
}

void CallLogWrapper::glProgramUniform2fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2fv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 2)) << ");" << TestLog::EndMessage;
	m_gl.programUniform2fv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform2i (glw::GLuint program, glw::GLint location, glw::GLint v0, glw::GLint v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2i(" << program << ", " << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.programUniform2i(program, location, v0, v1);
}

void CallLogWrapper::glProgramUniform2iv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2iv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 2)) << ");" << TestLog::EndMessage;
	m_gl.programUniform2iv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform2ui (glw::GLuint program, glw::GLint location, glw::GLuint v0, glw::GLuint v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2ui(" << program << ", " << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.programUniform2ui(program, location, v0, v1);
}

void CallLogWrapper::glProgramUniform2uiv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2uiv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 2)) << ");" << TestLog::EndMessage;
	m_gl.programUniform2uiv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform3d (glw::GLuint program, glw::GLint location, glw::GLdouble v0, glw::GLdouble v1, glw::GLdouble v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3d(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.programUniform3d(program, location, v0, v1, v2);
}

void CallLogWrapper::glProgramUniform3dEXT (glw::GLuint program, glw::GLint location, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3dEXT(" << program << ", " << location << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.programUniform3dEXT(program, location, x, y, z);
}

void CallLogWrapper::glProgramUniform3dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3dv(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform3dv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform3dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3dvEXT(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform3dvEXT(program, location, count, value);
}

void CallLogWrapper::glProgramUniform3f (glw::GLuint program, glw::GLint location, glw::GLfloat v0, glw::GLfloat v1, glw::GLfloat v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3f(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.programUniform3f(program, location, v0, v1, v2);
}

void CallLogWrapper::glProgramUniform3fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3fv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 3)) << ");" << TestLog::EndMessage;
	m_gl.programUniform3fv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform3i (glw::GLuint program, glw::GLint location, glw::GLint v0, glw::GLint v1, glw::GLint v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3i(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.programUniform3i(program, location, v0, v1, v2);
}

void CallLogWrapper::glProgramUniform3iv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3iv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 3)) << ");" << TestLog::EndMessage;
	m_gl.programUniform3iv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform3ui (glw::GLuint program, glw::GLint location, glw::GLuint v0, glw::GLuint v1, glw::GLuint v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3ui(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.programUniform3ui(program, location, v0, v1, v2);
}

void CallLogWrapper::glProgramUniform3uiv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3uiv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 3)) << ");" << TestLog::EndMessage;
	m_gl.programUniform3uiv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform4d (glw::GLuint program, glw::GLint location, glw::GLdouble v0, glw::GLdouble v1, glw::GLdouble v2, glw::GLdouble v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4d(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.programUniform4d(program, location, v0, v1, v2, v3);
}

void CallLogWrapper::glProgramUniform4dEXT (glw::GLuint program, glw::GLint location, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z, glw::GLdouble w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4dEXT(" << program << ", " << location << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.programUniform4dEXT(program, location, x, y, z, w);
}

void CallLogWrapper::glProgramUniform4dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4dv(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform4dv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform4dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4dvEXT(" << program << ", " << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniform4dvEXT(program, location, count, value);
}

void CallLogWrapper::glProgramUniform4f (glw::GLuint program, glw::GLint location, glw::GLfloat v0, glw::GLfloat v1, glw::GLfloat v2, glw::GLfloat v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4f(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.programUniform4f(program, location, v0, v1, v2, v3);
}

void CallLogWrapper::glProgramUniform4fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4fv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 4)) << ");" << TestLog::EndMessage;
	m_gl.programUniform4fv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform4i (glw::GLuint program, glw::GLint location, glw::GLint v0, glw::GLint v1, glw::GLint v2, glw::GLint v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4i(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.programUniform4i(program, location, v0, v1, v2, v3);
}

void CallLogWrapper::glProgramUniform4iv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4iv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 4)) << ");" << TestLog::EndMessage;
	m_gl.programUniform4iv(program, location, count, value);
}

void CallLogWrapper::glProgramUniform4ui (glw::GLuint program, glw::GLint location, glw::GLuint v0, glw::GLuint v1, glw::GLuint v2, glw::GLuint v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4ui(" << program << ", " << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.programUniform4ui(program, location, v0, v1, v2, v3);
}

void CallLogWrapper::glProgramUniform4uiv (glw::GLuint program, glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4uiv(" << program << ", " << location << ", " << count << ", " << getPointerStr(value, (count * 4)) << ");" << TestLog::EndMessage;
	m_gl.programUniform4uiv(program, location, count, value);
}

void CallLogWrapper::glProgramUniformMatrix2dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 2*2)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2x3dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x3dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x3dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2x3dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x3dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x3dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2x3fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x3fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 2*3)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x3fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2x4dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x4dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x4dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2x4dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x4dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x4dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix2x4fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x4fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 2*4)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x4fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 3*3)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3x2dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x2dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x2dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3x2dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x2dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x2dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3x2fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x2fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 3*2)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x2fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3x4dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x4dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x4dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3x4dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x4dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x4dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix3x4fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x4fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 3*4)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x4fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 4*4)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4x2dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x2dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x2dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4x2dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x2dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x2dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4x2fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x2fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 4*2)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x2fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4x3dv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x3dv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x3dv(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4x3dvEXT (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x3dvEXT(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x3dvEXT(program, location, count, transpose, value);
}

void CallLogWrapper::glProgramUniformMatrix4x3fv (glw::GLuint program, glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x3fv(" << program << ", " << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 4*3)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x3fv(program, location, count, transpose, value);
}

void CallLogWrapper::glProvokingVertex (glw::GLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProvokingVertex(" << getProvokingVertexStr(mode) << ");" << TestLog::EndMessage;
	m_gl.provokingVertex(mode);
}

void CallLogWrapper::glPushClientAttribDefaultEXT (glw::GLbitfield mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPushClientAttribDefaultEXT(" << toHex(mask) << ");" << TestLog::EndMessage;
	m_gl.pushClientAttribDefaultEXT(mask);
}

void CallLogWrapper::glPushDebugGroup (glw::GLenum source, glw::GLuint id, glw::GLsizei length, const glw::GLchar *message)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPushDebugGroup(" << getDebugMessageSourceStr(source) << ", " << id << ", " << length << ", " << getStringStr(message) << ");" << TestLog::EndMessage;
	m_gl.pushDebugGroup(source, id, length, message);
}

void CallLogWrapper::glPushGroupMarkerEXT (glw::GLsizei length, const glw::GLchar *marker)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPushGroupMarkerEXT(" << length << ", " << getStringStr(marker) << ");" << TestLog::EndMessage;
	m_gl.pushGroupMarkerEXT(length, marker);
}

void CallLogWrapper::glQueryCounter (glw::GLuint id, glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glQueryCounter(" << id << ", " << toHex(target) << ");" << TestLog::EndMessage;
	m_gl.queryCounter(id, target);
}

void CallLogWrapper::glReadBuffer (glw::GLenum src)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReadBuffer(" << getDrawReadBufferStr(src) << ");" << TestLog::EndMessage;
	m_gl.readBuffer(src);
}

void CallLogWrapper::glReadPixels (glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLenum type, void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReadPixels(" << x << ", " << y << ", " << width << ", " << height << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.readPixels(x, y, width, height, format, type, pixels);
}

void CallLogWrapper::glReadnPixels (glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLenum type, glw::GLsizei bufSize, void *data)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReadnPixels(" << x << ", " << y << ", " << width << ", " << height << ", " << toHex(format) << ", " << toHex(type) << ", " << bufSize << ", " << data << ");" << TestLog::EndMessage;
	m_gl.readnPixels(x, y, width, height, format, type, bufSize, data);
}

void CallLogWrapper::glReleaseShaderCompiler (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReleaseShaderCompiler(" << ");" << TestLog::EndMessage;
	m_gl.releaseShaderCompiler();
}

void CallLogWrapper::glRenderGpuMaskNV (glw::GLbitfield mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glRenderGpuMaskNV(" << toHex(mask) << ");" << TestLog::EndMessage;
	m_gl.renderGpuMaskNV(mask);
}

void CallLogWrapper::glRenderbufferStorage (glw::GLenum target, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glRenderbufferStorage(" << getFramebufferTargetStr(target) << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.renderbufferStorage(target, internalformat, width, height);
}

void CallLogWrapper::glRenderbufferStorageMultisample (glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glRenderbufferStorageMultisample(" << getFramebufferTargetStr(target) << ", " << samples << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.renderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void CallLogWrapper::glRenderbufferStorageMultisampleEXT (glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glRenderbufferStorageMultisampleEXT(" << toHex(target) << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.renderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
}

void CallLogWrapper::glResumeTransformFeedback (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glResumeTransformFeedback(" << ");" << TestLog::EndMessage;
	m_gl.resumeTransformFeedback();
}

void CallLogWrapper::glSampleCoverage (glw::GLfloat value, glw::GLboolean invert)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSampleCoverage(" << value << ", " << getBooleanStr(invert) << ");" << TestLog::EndMessage;
	m_gl.sampleCoverage(value, invert);
}

void CallLogWrapper::glSampleMaski (glw::GLuint maskNumber, glw::GLbitfield mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSampleMaski(" << maskNumber << ", " << toHex(mask) << ");" << TestLog::EndMessage;
	m_gl.sampleMaski(maskNumber, mask);
}

void CallLogWrapper::glSamplerParameterIiv (glw::GLuint sampler, glw::GLenum pname, const glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterIiv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(param, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.samplerParameterIiv(sampler, pname, param);
}

void CallLogWrapper::glSamplerParameterIuiv (glw::GLuint sampler, glw::GLenum pname, const glw::GLuint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterIuiv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(param, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.samplerParameterIuiv(sampler, pname, param);
}

void CallLogWrapper::glSamplerParameterf (glw::GLuint sampler, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterf(" << sampler << ", " << getTextureParameterStr(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.samplerParameterf(sampler, pname, param);
}

void CallLogWrapper::glSamplerParameterfv (glw::GLuint sampler, glw::GLenum pname, const glw::GLfloat *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterfv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(param, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.samplerParameterfv(sampler, pname, param);
}

void CallLogWrapper::glSamplerParameteri (glw::GLuint sampler, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameteri(" << sampler << ", " << getTextureParameterStr(pname) << ", " << getTextureParameterValueStr(pname, param) << ");" << TestLog::EndMessage;
	m_gl.samplerParameteri(sampler, pname, param);
}

void CallLogWrapper::glSamplerParameteriv (glw::GLuint sampler, glw::GLenum pname, const glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameteriv(" << sampler << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(param, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.samplerParameteriv(sampler, pname, param);
}

void CallLogWrapper::glScissor (glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissor(" << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.scissor(x, y, width, height);
}

void CallLogWrapper::glScissorArrayv (glw::GLuint first, glw::GLsizei count, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissorArrayv(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.scissorArrayv(first, count, v);
}

void CallLogWrapper::glScissorIndexed (glw::GLuint index, glw::GLint left, glw::GLint bottom, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissorIndexed(" << index << ", " << left << ", " << bottom << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.scissorIndexed(index, left, bottom, width, height);
}

void CallLogWrapper::glScissorIndexedv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissorIndexedv(" << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.scissorIndexedv(index, v);
}

void CallLogWrapper::glShaderBinary (glw::GLsizei count, const glw::GLuint *shaders, glw::GLenum binaryFormat, const void *binary, glw::GLsizei length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShaderBinary(" << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(shaders))) << ", " << toHex(binaryFormat) << ", " << binary << ", " << length << ");" << TestLog::EndMessage;
	m_gl.shaderBinary(count, shaders, binaryFormat, binary, length);
}

void CallLogWrapper::glShaderSource (glw::GLuint shader, glw::GLsizei count, const glw::GLchar *const*string, const glw::GLint *length)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShaderSource(" << shader << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(string))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(length))) << ");" << TestLog::EndMessage;
	m_gl.shaderSource(shader, count, string, length);
}

void CallLogWrapper::glShaderStorageBlockBinding (glw::GLuint program, glw::GLuint storageBlockIndex, glw::GLuint storageBlockBinding)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShaderStorageBlockBinding(" << program << ", " << storageBlockIndex << ", " << storageBlockBinding << ");" << TestLog::EndMessage;
	m_gl.shaderStorageBlockBinding(program, storageBlockIndex, storageBlockBinding);
}

void CallLogWrapper::glShadingRateEXT (glw::GLenum rate)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShadingRateEXT(" << toHex(rate) << ");" << TestLog::EndMessage;
	m_gl.shadingRateEXT(rate);
}

void CallLogWrapper::glShadingRateCombinerOpsEXT (glw::GLenum combinerOp0, glw::GLenum combinerOp1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShadingRateCombinerOpsEXT(" << toHex(combinerOp0) << ", " << toHex(combinerOp1) << ");" << TestLog::EndMessage;
	m_gl.shadingRateCombinerOpsEXT(combinerOp0, combinerOp1);
}

void CallLogWrapper::glSpecializeShader (glw::GLuint shader, const glw::GLchar *pEntryPoint, glw::GLuint numSpecializationConstants, const glw::GLuint *pConstantIndex, const glw::GLuint *pConstantValue)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSpecializeShader(" << shader << ", " << getStringStr(pEntryPoint) << ", " << numSpecializationConstants << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(pConstantIndex))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(pConstantValue))) << ");" << TestLog::EndMessage;
	m_gl.specializeShader(shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

void CallLogWrapper::glStencilFunc (glw::GLenum func, glw::GLint ref, glw::GLuint mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilFunc(" << getCompareFuncStr(func) << ", " << ref << ", " << mask << ");" << TestLog::EndMessage;
	m_gl.stencilFunc(func, ref, mask);
}

void CallLogWrapper::glStencilFuncSeparate (glw::GLenum face, glw::GLenum func, glw::GLint ref, glw::GLuint mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilFuncSeparate(" << getFaceStr(face) << ", " << getCompareFuncStr(func) << ", " << ref << ", " << mask << ");" << TestLog::EndMessage;
	m_gl.stencilFuncSeparate(face, func, ref, mask);
}

void CallLogWrapper::glStencilMask (glw::GLuint mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilMask(" << mask << ");" << TestLog::EndMessage;
	m_gl.stencilMask(mask);
}

void CallLogWrapper::glStencilMaskSeparate (glw::GLenum face, glw::GLuint mask)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilMaskSeparate(" << getFaceStr(face) << ", " << mask << ");" << TestLog::EndMessage;
	m_gl.stencilMaskSeparate(face, mask);
}

void CallLogWrapper::glStencilOp (glw::GLenum fail, glw::GLenum zfail, glw::GLenum zpass)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilOp(" << getStencilOpStr(fail) << ", " << getStencilOpStr(zfail) << ", " << getStencilOpStr(zpass) << ");" << TestLog::EndMessage;
	m_gl.stencilOp(fail, zfail, zpass);
}

void CallLogWrapper::glStencilOpSeparate (glw::GLenum face, glw::GLenum sfail, glw::GLenum dpfail, glw::GLenum dppass)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilOpSeparate(" << getFaceStr(face) << ", " << getStencilOpStr(sfail) << ", " << getStencilOpStr(dpfail) << ", " << getStencilOpStr(dppass) << ");" << TestLog::EndMessage;
	m_gl.stencilOpSeparate(face, sfail, dpfail, dppass);
}

void CallLogWrapper::glTexBuffer (glw::GLenum target, glw::GLenum internalformat, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexBuffer(" << getBufferTargetStr(target) << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.texBuffer(target, internalformat, buffer);
}

void CallLogWrapper::glTexBufferRange (glw::GLenum target, glw::GLenum internalformat, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexBufferRange(" << getBufferTargetStr(target) << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << buffer << ", " << offset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.texBufferRange(target, internalformat, buffer, offset, size);
}

void CallLogWrapper::glTexImage1D (glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage1D(" << getTextureTargetStr(target) << ", " << level << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << border << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texImage1D(target, level, internalformat, width, border, format, type, pixels);
}

void CallLogWrapper::glTexImage2D (glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage2D(" << getTextureTargetStr(target) << ", " << level << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << border << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void CallLogWrapper::glTexImage2DMultisample (glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage2DMultisample(" << getTextureTargetStr(target) << ", " << samples << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.texImage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
}

void CallLogWrapper::glTexImage3D (glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage3D(" << getTextureTargetStr(target) << ", " << level << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void CallLogWrapper::glTexImage3DMultisample (glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage3DMultisample(" << toHex(target) << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.texImage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void CallLogWrapper::glTexImage3DOES (glw::GLenum target, glw::GLint level, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage3DOES(" << toHex(target) << ", " << level << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texImage3DOES(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void CallLogWrapper::glTexPageCommitmentARB (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLboolean commit)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexPageCommitmentARB(" << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << getBooleanStr(commit) << ");" << TestLog::EndMessage;
	m_gl.texPageCommitmentARB(target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void CallLogWrapper::glTexParameterIiv (glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterIiv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(params, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.texParameterIiv(target, pname, params);
}

void CallLogWrapper::glTexParameterIuiv (glw::GLenum target, glw::GLenum pname, const glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterIuiv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(params, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.texParameterIuiv(target, pname, params);
}

void CallLogWrapper::glTexParameterf (glw::GLenum target, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterf(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.texParameterf(target, pname, param);
}

void CallLogWrapper::glTexParameterfv (glw::GLenum target, glw::GLenum pname, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterfv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(params, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.texParameterfv(target, pname, params);
}

void CallLogWrapper::glTexParameteri (glw::GLenum target, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameteri(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << getTextureParameterValueStr(pname, param) << ");" << TestLog::EndMessage;
	m_gl.texParameteri(target, pname, param);
}

void CallLogWrapper::glTexParameteriv (glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameteriv(" << getTextureTargetStr(target) << ", " << getTextureParameterStr(pname) << ", " << getPointerStr(params, getTextureParamNumArgs(pname)) << ");" << TestLog::EndMessage;
	m_gl.texParameteriv(target, pname, params);
}

void CallLogWrapper::glTexStorage1D (glw::GLenum target, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage1D(" << toHex(target) << ", " << levels << ", " << toHex(internalformat) << ", " << width << ");" << TestLog::EndMessage;
	m_gl.texStorage1D(target, levels, internalformat, width);
}

void CallLogWrapper::glTexStorage2D (glw::GLenum target, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage2D(" << getTextureTargetStr(target) << ", " << levels << ", " << getTextureFormatStr(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.texStorage2D(target, levels, internalformat, width, height);
}

void CallLogWrapper::glTexStorage2DMultisample (glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage2DMultisample(" << getTextureTargetStr(target) << ", " << samples << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.texStorage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
}

void CallLogWrapper::glTexStorage3D (glw::GLenum target, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage3D(" << getTextureTargetStr(target) << ", " << levels << ", " << getTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << depth << ");" << TestLog::EndMessage;
	m_gl.texStorage3D(target, levels, internalformat, width, height, depth);
}

void CallLogWrapper::glTexStorage3DMultisample (glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage3DMultisample(" << getTextureTargetStr(target) << ", " << samples << ", " << getUncompressedTextureFormatStr(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.texStorage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void CallLogWrapper::glTexSubImage1D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage1D(" << getTextureTargetStr(target) << ", " << level << ", " << xoffset << ", " << width << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texSubImage1D(target, level, xoffset, width, format, type, pixels);
}

void CallLogWrapper::glTexSubImage2D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage2D(" << getTextureTargetStr(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void CallLogWrapper::glTexSubImage3D (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage3D(" << getTextureTargetStr(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << getUncompressedTextureFormatStr(format) << ", " << getTypeStr(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void CallLogWrapper::glTexSubImage3DOES (glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage3DOES(" << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.texSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void CallLogWrapper::glTextureBarrier (void)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureBarrier(" << ");" << TestLog::EndMessage;
	m_gl.textureBarrier();
}

void CallLogWrapper::glTextureBuffer (glw::GLuint texture, glw::GLenum internalformat, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureBuffer(" << texture << ", " << toHex(internalformat) << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.textureBuffer(texture, internalformat, buffer);
}

void CallLogWrapper::glTextureBufferEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum internalformat, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureBufferEXT(" << texture << ", " << toHex(target) << ", " << toHex(internalformat) << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.textureBufferEXT(texture, target, internalformat, buffer);
}

void CallLogWrapper::glTextureBufferRange (glw::GLuint texture, glw::GLenum internalformat, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureBufferRange(" << texture << ", " << toHex(internalformat) << ", " << buffer << ", " << offset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.textureBufferRange(texture, internalformat, buffer, offset, size);
}

void CallLogWrapper::glTextureBufferRangeEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum internalformat, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureBufferRangeEXT(" << texture << ", " << toHex(target) << ", " << toHex(internalformat) << ", " << buffer << ", " << offset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.textureBufferRangeEXT(texture, target, internalformat, buffer, offset, size);
}

void CallLogWrapper::glTextureImage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureImage1DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << internalformat << ", " << width << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureImage1DEXT(texture, target, level, internalformat, width, border, format, type, pixels);
}

void CallLogWrapper::glTextureImage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureImage2DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << internalformat << ", " << width << ", " << height << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureImage2DEXT(texture, target, level, internalformat, width, height, border, format, type, pixels);
}

void CallLogWrapper::glTextureImage3DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLint border, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureImage3DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << internalformat << ", " << width << ", " << height << ", " << depth << ", " << border << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void CallLogWrapper::glTexturePageCommitmentEXT (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLboolean commit)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexturePageCommitmentEXT(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << getBooleanStr(commit) << ");" << TestLog::EndMessage;
	m_gl.texturePageCommitmentEXT(texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void CallLogWrapper::glTextureParameterIiv (glw::GLuint texture, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterIiv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterIiv(texture, pname, params);
}

void CallLogWrapper::glTextureParameterIivEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterIivEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterIivEXT(texture, target, pname, params);
}

void CallLogWrapper::glTextureParameterIuiv (glw::GLuint texture, glw::GLenum pname, const glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterIuiv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterIuiv(texture, pname, params);
}

void CallLogWrapper::glTextureParameterIuivEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, const glw::GLuint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterIuivEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterIuivEXT(texture, target, pname, params);
}

void CallLogWrapper::glTextureParameterf (glw::GLuint texture, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterf(" << texture << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.textureParameterf(texture, pname, param);
}

void CallLogWrapper::glTextureParameterfEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, glw::GLfloat param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterfEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.textureParameterfEXT(texture, target, pname, param);
}

void CallLogWrapper::glTextureParameterfv (glw::GLuint texture, glw::GLenum pname, const glw::GLfloat *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterfv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterfv(texture, pname, param);
}

void CallLogWrapper::glTextureParameterfvEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, const glw::GLfloat *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterfvEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterfvEXT(texture, target, pname, params);
}

void CallLogWrapper::glTextureParameteri (glw::GLuint texture, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameteri(" << texture << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.textureParameteri(texture, pname, param);
}

void CallLogWrapper::glTextureParameteriEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, glw::GLint param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameteriEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << param << ");" << TestLog::EndMessage;
	m_gl.textureParameteriEXT(texture, target, pname, param);
}

void CallLogWrapper::glTextureParameteriv (glw::GLuint texture, glw::GLenum pname, const glw::GLint *param)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameteriv(" << texture << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(param))) << ");" << TestLog::EndMessage;
	m_gl.textureParameteriv(texture, pname, param);
}

void CallLogWrapper::glTextureParameterivEXT (glw::GLuint texture, glw::GLenum target, glw::GLenum pname, const glw::GLint *params)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureParameterivEXT(" << texture << ", " << toHex(target) << ", " << toHex(pname) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(params))) << ");" << TestLog::EndMessage;
	m_gl.textureParameterivEXT(texture, target, pname, params);
}

void CallLogWrapper::glTextureRenderbufferEXT (glw::GLuint texture, glw::GLenum target, glw::GLuint renderbuffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureRenderbufferEXT(" << texture << ", " << toHex(target) << ", " << renderbuffer << ");" << TestLog::EndMessage;
	m_gl.textureRenderbufferEXT(texture, target, renderbuffer);
}

void CallLogWrapper::glTextureStorage1D (glw::GLuint texture, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage1D(" << texture << ", " << levels << ", " << toHex(internalformat) << ", " << width << ");" << TestLog::EndMessage;
	m_gl.textureStorage1D(texture, levels, internalformat, width);
}

void CallLogWrapper::glTextureStorage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage1DEXT(" << texture << ", " << toHex(target) << ", " << levels << ", " << toHex(internalformat) << ", " << width << ");" << TestLog::EndMessage;
	m_gl.textureStorage1DEXT(texture, target, levels, internalformat, width);
}

void CallLogWrapper::glTextureStorage2D (glw::GLuint texture, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage2D(" << texture << ", " << levels << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.textureStorage2D(texture, levels, internalformat, width, height);
}

void CallLogWrapper::glTextureStorage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage2DEXT(" << texture << ", " << toHex(target) << ", " << levels << ", " << toHex(internalformat) << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.textureStorage2DEXT(texture, target, levels, internalformat, width, height);
}

void CallLogWrapper::glTextureStorage2DMultisample (glw::GLuint texture, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage2DMultisample(" << texture << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.textureStorage2DMultisample(texture, samples, internalformat, width, height, fixedsamplelocations);
}

void CallLogWrapper::glTextureStorage2DMultisampleEXT (glw::GLuint texture, glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage2DMultisampleEXT(" << texture << ", " << toHex(target) << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.textureStorage2DMultisampleEXT(texture, target, samples, internalformat, width, height, fixedsamplelocations);
}

void CallLogWrapper::glTextureStorage3D (glw::GLuint texture, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage3D(" << texture << ", " << levels << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ");" << TestLog::EndMessage;
	m_gl.textureStorage3D(texture, levels, internalformat, width, height, depth);
}

void CallLogWrapper::glTextureStorage3DEXT (glw::GLuint texture, glw::GLenum target, glw::GLsizei levels, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage3DEXT(" << texture << ", " << toHex(target) << ", " << levels << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ");" << TestLog::EndMessage;
	m_gl.textureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
}

void CallLogWrapper::glTextureStorage3DMultisample (glw::GLuint texture, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage3DMultisample(" << texture << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.textureStorage3DMultisample(texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void CallLogWrapper::glTextureStorage3DMultisampleEXT (glw::GLuint texture, glw::GLenum target, glw::GLsizei samples, glw::GLenum internalformat, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLboolean fixedsamplelocations)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureStorage3DMultisampleEXT(" << texture << ", " << toHex(target) << ", " << samples << ", " << toHex(internalformat) << ", " << width << ", " << height << ", " << depth << ", " << getBooleanStr(fixedsamplelocations) << ");" << TestLog::EndMessage;
	m_gl.textureStorage3DMultisampleEXT(texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void CallLogWrapper::glTextureSubImage1D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureSubImage1D(" << texture << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureSubImage1D(texture, level, xoffset, width, format, type, pixels);
}

void CallLogWrapper::glTextureSubImage1DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLsizei width, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureSubImage1DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << width << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels);
}

void CallLogWrapper::glTextureSubImage2D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureSubImage2D(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);
}

void CallLogWrapper::glTextureSubImage2DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLsizei width, glw::GLsizei height, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureSubImage2DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void CallLogWrapper::glTextureSubImage3D (glw::GLuint texture, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureSubImage3D(" << texture << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void CallLogWrapper::glTextureSubImage3DEXT (glw::GLuint texture, glw::GLenum target, glw::GLint level, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format, glw::GLenum type, const void *pixels)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureSubImage3DEXT(" << texture << ", " << toHex(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << zoffset << ", " << width << ", " << height << ", " << depth << ", " << toHex(format) << ", " << toHex(type) << ", " << pixels << ");" << TestLog::EndMessage;
	m_gl.textureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void CallLogWrapper::glTextureView (glw::GLuint texture, glw::GLenum target, glw::GLuint origtexture, glw::GLenum internalformat, glw::GLuint minlevel, glw::GLuint numlevels, glw::GLuint minlayer, glw::GLuint numlayers)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureView(" << texture << ", " << toHex(target) << ", " << origtexture << ", " << toHex(internalformat) << ", " << minlevel << ", " << numlevels << ", " << minlayer << ", " << numlayers << ");" << TestLog::EndMessage;
	m_gl.textureView(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

void CallLogWrapper::glTransformFeedbackBufferBase (glw::GLuint xfb, glw::GLuint index, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTransformFeedbackBufferBase(" << xfb << ", " << index << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.transformFeedbackBufferBase(xfb, index, buffer);
}

void CallLogWrapper::glTransformFeedbackBufferRange (glw::GLuint xfb, glw::GLuint index, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizeiptr size)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTransformFeedbackBufferRange(" << xfb << ", " << index << ", " << buffer << ", " << offset << ", " << size << ");" << TestLog::EndMessage;
	m_gl.transformFeedbackBufferRange(xfb, index, buffer, offset, size);
}

void CallLogWrapper::glTransformFeedbackVaryings (glw::GLuint program, glw::GLsizei count, const glw::GLchar *const*varyings, glw::GLenum bufferMode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTransformFeedbackVaryings(" << program << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(varyings))) << ", " << toHex(bufferMode) << ");" << TestLog::EndMessage;
	m_gl.transformFeedbackVaryings(program, count, varyings, bufferMode);
}

void CallLogWrapper::glUniform1d (glw::GLint location, glw::GLdouble x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1d(" << location << ", " << x << ");" << TestLog::EndMessage;
	m_gl.uniform1d(location, x);
}

void CallLogWrapper::glUniform1dv (glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1dv(" << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniform1dv(location, count, value);
}

void CallLogWrapper::glUniform1f (glw::GLint location, glw::GLfloat v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1f(" << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.uniform1f(location, v0);
}

void CallLogWrapper::glUniform1fv (glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1fv(" << location << ", " << count << ", " << getPointerStr(value, (count * 1)) << ");" << TestLog::EndMessage;
	m_gl.uniform1fv(location, count, value);
}

void CallLogWrapper::glUniform1i (glw::GLint location, glw::GLint v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1i(" << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.uniform1i(location, v0);
}

void CallLogWrapper::glUniform1iv (glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1iv(" << location << ", " << count << ", " << getPointerStr(value, (count * 1)) << ");" << TestLog::EndMessage;
	m_gl.uniform1iv(location, count, value);
}

void CallLogWrapper::glUniform1ui (glw::GLint location, glw::GLuint v0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1ui(" << location << ", " << v0 << ");" << TestLog::EndMessage;
	m_gl.uniform1ui(location, v0);
}

void CallLogWrapper::glUniform1uiv (glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1uiv(" << location << ", " << count << ", " << getPointerStr(value, (count * 1)) << ");" << TestLog::EndMessage;
	m_gl.uniform1uiv(location, count, value);
}

void CallLogWrapper::glUniform2d (glw::GLint location, glw::GLdouble x, glw::GLdouble y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2d(" << location << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.uniform2d(location, x, y);
}

void CallLogWrapper::glUniform2dv (glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2dv(" << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniform2dv(location, count, value);
}

void CallLogWrapper::glUniform2f (glw::GLint location, glw::GLfloat v0, glw::GLfloat v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2f(" << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.uniform2f(location, v0, v1);
}

void CallLogWrapper::glUniform2fv (glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2fv(" << location << ", " << count << ", " << getPointerStr(value, (count * 2)) << ");" << TestLog::EndMessage;
	m_gl.uniform2fv(location, count, value);
}

void CallLogWrapper::glUniform2i (glw::GLint location, glw::GLint v0, glw::GLint v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2i(" << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.uniform2i(location, v0, v1);
}

void CallLogWrapper::glUniform2iv (glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2iv(" << location << ", " << count << ", " << getPointerStr(value, (count * 2)) << ");" << TestLog::EndMessage;
	m_gl.uniform2iv(location, count, value);
}

void CallLogWrapper::glUniform2ui (glw::GLint location, glw::GLuint v0, glw::GLuint v1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2ui(" << location << ", " << v0 << ", " << v1 << ");" << TestLog::EndMessage;
	m_gl.uniform2ui(location, v0, v1);
}

void CallLogWrapper::glUniform2uiv (glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2uiv(" << location << ", " << count << ", " << getPointerStr(value, (count * 2)) << ");" << TestLog::EndMessage;
	m_gl.uniform2uiv(location, count, value);
}

void CallLogWrapper::glUniform3d (glw::GLint location, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3d(" << location << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.uniform3d(location, x, y, z);
}

void CallLogWrapper::glUniform3dv (glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3dv(" << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniform3dv(location, count, value);
}

void CallLogWrapper::glUniform3f (glw::GLint location, glw::GLfloat v0, glw::GLfloat v1, glw::GLfloat v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3f(" << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.uniform3f(location, v0, v1, v2);
}

void CallLogWrapper::glUniform3fv (glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3fv(" << location << ", " << count << ", " << getPointerStr(value, (count * 3)) << ");" << TestLog::EndMessage;
	m_gl.uniform3fv(location, count, value);
}

void CallLogWrapper::glUniform3i (glw::GLint location, glw::GLint v0, glw::GLint v1, glw::GLint v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3i(" << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.uniform3i(location, v0, v1, v2);
}

void CallLogWrapper::glUniform3iv (glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3iv(" << location << ", " << count << ", " << getPointerStr(value, (count * 3)) << ");" << TestLog::EndMessage;
	m_gl.uniform3iv(location, count, value);
}

void CallLogWrapper::glUniform3ui (glw::GLint location, glw::GLuint v0, glw::GLuint v1, glw::GLuint v2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3ui(" << location << ", " << v0 << ", " << v1 << ", " << v2 << ");" << TestLog::EndMessage;
	m_gl.uniform3ui(location, v0, v1, v2);
}

void CallLogWrapper::glUniform3uiv (glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3uiv(" << location << ", " << count << ", " << getPointerStr(value, (count * 3)) << ");" << TestLog::EndMessage;
	m_gl.uniform3uiv(location, count, value);
}

void CallLogWrapper::glUniform4d (glw::GLint location, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z, glw::GLdouble w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4d(" << location << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.uniform4d(location, x, y, z, w);
}

void CallLogWrapper::glUniform4dv (glw::GLint location, glw::GLsizei count, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4dv(" << location << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniform4dv(location, count, value);
}

void CallLogWrapper::glUniform4f (glw::GLint location, glw::GLfloat v0, glw::GLfloat v1, glw::GLfloat v2, glw::GLfloat v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4f(" << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.uniform4f(location, v0, v1, v2, v3);
}

void CallLogWrapper::glUniform4fv (glw::GLint location, glw::GLsizei count, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4fv(" << location << ", " << count << ", " << getPointerStr(value, (count * 4)) << ");" << TestLog::EndMessage;
	m_gl.uniform4fv(location, count, value);
}

void CallLogWrapper::glUniform4i (glw::GLint location, glw::GLint v0, glw::GLint v1, glw::GLint v2, glw::GLint v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4i(" << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.uniform4i(location, v0, v1, v2, v3);
}

void CallLogWrapper::glUniform4iv (glw::GLint location, glw::GLsizei count, const glw::GLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4iv(" << location << ", " << count << ", " << getPointerStr(value, (count * 4)) << ");" << TestLog::EndMessage;
	m_gl.uniform4iv(location, count, value);
}

void CallLogWrapper::glUniform4ui (glw::GLint location, glw::GLuint v0, glw::GLuint v1, glw::GLuint v2, glw::GLuint v3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4ui(" << location << ", " << v0 << ", " << v1 << ", " << v2 << ", " << v3 << ");" << TestLog::EndMessage;
	m_gl.uniform4ui(location, v0, v1, v2, v3);
}

void CallLogWrapper::glUniform4uiv (glw::GLint location, glw::GLsizei count, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4uiv(" << location << ", " << count << ", " << getPointerStr(value, (count * 4)) << ");" << TestLog::EndMessage;
	m_gl.uniform4uiv(location, count, value);
}

void CallLogWrapper::glUniformBlockBinding (glw::GLuint program, glw::GLuint uniformBlockIndex, glw::GLuint uniformBlockBinding)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformBlockBinding(" << program << ", " << uniformBlockIndex << ", " << uniformBlockBinding << ");" << TestLog::EndMessage;
	m_gl.uniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

void CallLogWrapper::glUniformMatrix2dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix2fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 2*2)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix2x3dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x3dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x3dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix2x3fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x3fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 2*3)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x3fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix2x4dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x4dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x4dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix2x4fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x4fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 2*4)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x4fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix3dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix3fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 3*3)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix3x2dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x2dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x2dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix3x2fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x2fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 3*2)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x2fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix3x4dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x4dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x4dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix3x4fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x4fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 3*4)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x4fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix4dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix4fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 4*4)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix4x2dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x2dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x2dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix4x2fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x2fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 4*2)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x2fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix4x3dv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLdouble *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x3dv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x3dv(location, count, transpose, value);
}

void CallLogWrapper::glUniformMatrix4x3fv (glw::GLint location, glw::GLsizei count, glw::GLboolean transpose, const glw::GLfloat *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x3fv(" << location << ", " << count << ", " << getBooleanStr(transpose) << ", " << getPointerStr(value, (count * 4*3)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x3fv(location, count, transpose, value);
}

void CallLogWrapper::glUniformSubroutinesuiv (glw::GLenum shadertype, glw::GLsizei count, const glw::GLuint *indices)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformSubroutinesuiv(" << toHex(shadertype) << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(indices))) << ");" << TestLog::EndMessage;
	m_gl.uniformSubroutinesuiv(shadertype, count, indices);
}

glw::GLboolean CallLogWrapper::glUnmapBuffer (glw::GLenum target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUnmapBuffer(" << getBufferTargetStr(target) << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.unmapBuffer(target);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glUnmapNamedBuffer (glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUnmapNamedBuffer(" << buffer << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.unmapNamedBuffer(buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glUnmapNamedBufferEXT (glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUnmapNamedBufferEXT(" << buffer << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.unmapNamedBufferEXT(buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glUseProgram (glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUseProgram(" << program << ");" << TestLog::EndMessage;
	m_gl.useProgram(program);
}

void CallLogWrapper::glUseProgramStages (glw::GLuint pipeline, glw::GLbitfield stages, glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUseProgramStages(" << pipeline << ", " << getShaderTypeMaskStr(stages) << ", " << program << ");" << TestLog::EndMessage;
	m_gl.useProgramStages(pipeline, stages, program);
}

void CallLogWrapper::glValidateProgram (glw::GLuint program)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glValidateProgram(" << program << ");" << TestLog::EndMessage;
	m_gl.validateProgram(program);
}

void CallLogWrapper::glValidateProgramPipeline (glw::GLuint pipeline)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glValidateProgramPipeline(" << pipeline << ");" << TestLog::EndMessage;
	m_gl.validateProgramPipeline(pipeline);
}

void CallLogWrapper::glVertexArrayAttribBinding (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLuint bindingindex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayAttribBinding(" << vaobj << ", " << attribindex << ", " << bindingindex << ");" << TestLog::EndMessage;
	m_gl.vertexArrayAttribBinding(vaobj, attribindex, bindingindex);
}

void CallLogWrapper::glVertexArrayAttribFormat (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLboolean normalized, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayAttribFormat(" << vaobj << ", " << attribindex << ", " << size << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayAttribFormat(vaobj, attribindex, size, type, normalized, relativeoffset);
}

void CallLogWrapper::glVertexArrayAttribIFormat (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayAttribIFormat(" << vaobj << ", " << attribindex << ", " << size << ", " << toHex(type) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayAttribIFormat(vaobj, attribindex, size, type, relativeoffset);
}

void CallLogWrapper::glVertexArrayAttribLFormat (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayAttribLFormat(" << vaobj << ", " << attribindex << ", " << size << ", " << toHex(type) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayAttribLFormat(vaobj, attribindex, size, type, relativeoffset);
}

void CallLogWrapper::glVertexArrayBindVertexBufferEXT (glw::GLuint vaobj, glw::GLuint bindingindex, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayBindVertexBufferEXT(" << vaobj << ", " << bindingindex << ", " << buffer << ", " << offset << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.vertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride);
}

void CallLogWrapper::glVertexArrayBindingDivisor (glw::GLuint vaobj, glw::GLuint bindingindex, glw::GLuint divisor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayBindingDivisor(" << vaobj << ", " << bindingindex << ", " << divisor << ");" << TestLog::EndMessage;
	m_gl.vertexArrayBindingDivisor(vaobj, bindingindex, divisor);
}

void CallLogWrapper::glVertexArrayColorOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayColorOffsetEXT(" << vaobj << ", " << buffer << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayColorOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void CallLogWrapper::glVertexArrayEdgeFlagOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayEdgeFlagOffsetEXT(" << vaobj << ", " << buffer << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayEdgeFlagOffsetEXT(vaobj, buffer, stride, offset);
}

void CallLogWrapper::glVertexArrayElementBuffer (glw::GLuint vaobj, glw::GLuint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayElementBuffer(" << vaobj << ", " << buffer << ");" << TestLog::EndMessage;
	m_gl.vertexArrayElementBuffer(vaobj, buffer);
}

void CallLogWrapper::glVertexArrayFogCoordOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayFogCoordOffsetEXT(" << vaobj << ", " << buffer << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayFogCoordOffsetEXT(vaobj, buffer, type, stride, offset);
}

void CallLogWrapper::glVertexArrayIndexOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayIndexOffsetEXT(" << vaobj << ", " << buffer << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayIndexOffsetEXT(vaobj, buffer, type, stride, offset);
}

void CallLogWrapper::glVertexArrayMultiTexCoordOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLenum texunit, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayMultiTexCoordOffsetEXT(" << vaobj << ", " << buffer << ", " << toHex(texunit) << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayMultiTexCoordOffsetEXT(vaobj, buffer, texunit, size, type, stride, offset);
}

void CallLogWrapper::glVertexArrayNormalOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayNormalOffsetEXT(" << vaobj << ", " << buffer << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayNormalOffsetEXT(vaobj, buffer, type, stride, offset);
}

void CallLogWrapper::glVertexArraySecondaryColorOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArraySecondaryColorOffsetEXT(" << vaobj << ", " << buffer << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArraySecondaryColorOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void CallLogWrapper::glVertexArrayTexCoordOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayTexCoordOffsetEXT(" << vaobj << ", " << buffer << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayTexCoordOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void CallLogWrapper::glVertexArrayVertexAttribBindingEXT (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLuint bindingindex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribBindingEXT(" << vaobj << ", " << attribindex << ", " << bindingindex << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribBindingEXT(vaobj, attribindex, bindingindex);
}

void CallLogWrapper::glVertexArrayVertexAttribDivisorEXT (glw::GLuint vaobj, glw::GLuint index, glw::GLuint divisor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribDivisorEXT(" << vaobj << ", " << index << ", " << divisor << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribDivisorEXT(vaobj, index, divisor);
}

void CallLogWrapper::glVertexArrayVertexAttribFormatEXT (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLboolean normalized, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribFormatEXT(" << vaobj << ", " << attribindex << ", " << size << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type, normalized, relativeoffset);
}

void CallLogWrapper::glVertexArrayVertexAttribIFormatEXT (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribIFormatEXT(" << vaobj << ", " << attribindex << ", " << size << ", " << toHex(type) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset);
}

void CallLogWrapper::glVertexArrayVertexAttribIOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLuint index, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribIOffsetEXT(" << vaobj << ", " << buffer << ", " << index << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
}

void CallLogWrapper::glVertexArrayVertexAttribLFormatEXT (glw::GLuint vaobj, glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribLFormatEXT(" << vaobj << ", " << attribindex << ", " << size << ", " << toHex(type) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset);
}

void CallLogWrapper::glVertexArrayVertexAttribLOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLuint index, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribLOffsetEXT(" << vaobj << ", " << buffer << ", " << index << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
}

void CallLogWrapper::glVertexArrayVertexAttribOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLuint index, glw::GLint size, glw::GLenum type, glw::GLboolean normalized, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexAttribOffsetEXT(" << vaobj << ", " << buffer << ", " << index << ", " << size << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type, normalized, stride, offset);
}

void CallLogWrapper::glVertexArrayVertexBindingDivisorEXT (glw::GLuint vaobj, glw::GLuint bindingindex, glw::GLuint divisor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexBindingDivisorEXT(" << vaobj << ", " << bindingindex << ", " << divisor << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor);
}

void CallLogWrapper::glVertexArrayVertexBuffer (glw::GLuint vaobj, glw::GLuint bindingindex, glw::GLuint buffer, glw::GLintptr offset, glw::GLsizei stride)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexBuffer(" << vaobj << ", " << bindingindex << ", " << buffer << ", " << offset << ", " << stride << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexBuffer(vaobj, bindingindex, buffer, offset, stride);
}

void CallLogWrapper::glVertexArrayVertexBuffers (glw::GLuint vaobj, glw::GLuint first, glw::GLsizei count, const glw::GLuint *buffers, const glw::GLintptr *offsets, const glw::GLsizei *strides)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexBuffers(" << vaobj << ", " << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(buffers))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(offsets))) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(strides))) << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexBuffers(vaobj, first, count, buffers, offsets, strides);
}

void CallLogWrapper::glVertexArrayVertexOffsetEXT (glw::GLuint vaobj, glw::GLuint buffer, glw::GLint size, glw::GLenum type, glw::GLsizei stride, glw::GLintptr offset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexArrayVertexOffsetEXT(" << vaobj << ", " << buffer << ", " << size << ", " << toHex(type) << ", " << stride << ", " << offset << ");" << TestLog::EndMessage;
	m_gl.vertexArrayVertexOffsetEXT(vaobj, buffer, size, type, stride, offset);
}

void CallLogWrapper::glVertexAttrib1d (glw::GLuint index, glw::GLdouble x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1d(" << index << ", " << x << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1d(index, x);
}

void CallLogWrapper::glVertexAttrib1dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1dv(" << index << ", " << getPointerStr(v, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1dv(index, v);
}

void CallLogWrapper::glVertexAttrib1f (glw::GLuint index, glw::GLfloat x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1f(" << index << ", " << x << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1f(index, x);
}

void CallLogWrapper::glVertexAttrib1fv (glw::GLuint index, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1fv(" << index << ", " << getPointerStr(v, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1fv(index, v);
}

void CallLogWrapper::glVertexAttrib1s (glw::GLuint index, glw::GLshort x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1s(" << index << ", " << x << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1s(index, x);
}

void CallLogWrapper::glVertexAttrib1sv (glw::GLuint index, const glw::GLshort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1sv(" << index << ", " << getPointerStr(v, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1sv(index, v);
}

void CallLogWrapper::glVertexAttrib2d (glw::GLuint index, glw::GLdouble x, glw::GLdouble y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2d(" << index << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2d(index, x, y);
}

void CallLogWrapper::glVertexAttrib2dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2dv(" << index << ", " << getPointerStr(v, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2dv(index, v);
}

void CallLogWrapper::glVertexAttrib2f (glw::GLuint index, glw::GLfloat x, glw::GLfloat y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2f(" << index << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2f(index, x, y);
}

void CallLogWrapper::glVertexAttrib2fv (glw::GLuint index, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2fv(" << index << ", " << getPointerStr(v, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2fv(index, v);
}

void CallLogWrapper::glVertexAttrib2s (glw::GLuint index, glw::GLshort x, glw::GLshort y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2s(" << index << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2s(index, x, y);
}

void CallLogWrapper::glVertexAttrib2sv (glw::GLuint index, const glw::GLshort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2sv(" << index << ", " << getPointerStr(v, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2sv(index, v);
}

void CallLogWrapper::glVertexAttrib3d (glw::GLuint index, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3d(" << index << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3d(index, x, y, z);
}

void CallLogWrapper::glVertexAttrib3dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3dv(" << index << ", " << getPointerStr(v, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3dv(index, v);
}

void CallLogWrapper::glVertexAttrib3f (glw::GLuint index, glw::GLfloat x, glw::GLfloat y, glw::GLfloat z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3f(" << index << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3f(index, x, y, z);
}

void CallLogWrapper::glVertexAttrib3fv (glw::GLuint index, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3fv(" << index << ", " << getPointerStr(v, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3fv(index, v);
}

void CallLogWrapper::glVertexAttrib3s (glw::GLuint index, glw::GLshort x, glw::GLshort y, glw::GLshort z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3s(" << index << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3s(index, x, y, z);
}

void CallLogWrapper::glVertexAttrib3sv (glw::GLuint index, const glw::GLshort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3sv(" << index << ", " << getPointerStr(v, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3sv(index, v);
}

void CallLogWrapper::glVertexAttrib4Nbv (glw::GLuint index, const glw::GLbyte *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nbv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nbv(index, v);
}

void CallLogWrapper::glVertexAttrib4Niv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Niv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Niv(index, v);
}

void CallLogWrapper::glVertexAttrib4Nsv (glw::GLuint index, const glw::GLshort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nsv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nsv(index, v);
}

void CallLogWrapper::glVertexAttrib4Nub (glw::GLuint index, glw::GLubyte x, glw::GLubyte y, glw::GLubyte z, glw::GLubyte w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nub(" << index << ", " << toHex(x) << ", " << toHex(y) << ", " << toHex(z) << ", " << toHex(w) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nub(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttrib4Nubv (glw::GLuint index, const glw::GLubyte *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nubv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nubv(index, v);
}

void CallLogWrapper::glVertexAttrib4Nuiv (glw::GLuint index, const glw::GLuint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nuiv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nuiv(index, v);
}

void CallLogWrapper::glVertexAttrib4Nusv (glw::GLuint index, const glw::GLushort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nusv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nusv(index, v);
}

void CallLogWrapper::glVertexAttrib4bv (glw::GLuint index, const glw::GLbyte *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4bv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4bv(index, v);
}

void CallLogWrapper::glVertexAttrib4d (glw::GLuint index, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z, glw::GLdouble w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4d(" << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4d(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttrib4dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4dv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4dv(index, v);
}

void CallLogWrapper::glVertexAttrib4f (glw::GLuint index, glw::GLfloat x, glw::GLfloat y, glw::GLfloat z, glw::GLfloat w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4f(" << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4f(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttrib4fv (glw::GLuint index, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4fv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4fv(index, v);
}

void CallLogWrapper::glVertexAttrib4iv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4iv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4iv(index, v);
}

void CallLogWrapper::glVertexAttrib4s (glw::GLuint index, glw::GLshort x, glw::GLshort y, glw::GLshort z, glw::GLshort w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4s(" << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4s(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttrib4sv (glw::GLuint index, const glw::GLshort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4sv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4sv(index, v);
}

void CallLogWrapper::glVertexAttrib4ubv (glw::GLuint index, const glw::GLubyte *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4ubv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4ubv(index, v);
}

void CallLogWrapper::glVertexAttrib4uiv (glw::GLuint index, const glw::GLuint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4uiv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4uiv(index, v);
}

void CallLogWrapper::glVertexAttrib4usv (glw::GLuint index, const glw::GLushort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4usv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4usv(index, v);
}

void CallLogWrapper::glVertexAttribBinding (glw::GLuint attribindex, glw::GLuint bindingindex)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribBinding(" << attribindex << ", " << bindingindex << ");" << TestLog::EndMessage;
	m_gl.vertexAttribBinding(attribindex, bindingindex);
}

void CallLogWrapper::glVertexAttribDivisor (glw::GLuint index, glw::GLuint divisor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribDivisor(" << index << ", " << divisor << ");" << TestLog::EndMessage;
	m_gl.vertexAttribDivisor(index, divisor);
}

void CallLogWrapper::glVertexAttribFormat (glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLboolean normalized, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribFormat(" << attribindex << ", " << size << ", " << getTypeStr(type) << ", " << getBooleanStr(normalized) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

void CallLogWrapper::glVertexAttribI1i (glw::GLuint index, glw::GLint x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1i(" << index << ", " << x << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1i(index, x);
}

void CallLogWrapper::glVertexAttribI1iv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1iv(" << index << ", " << getPointerStr(v, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1iv(index, v);
}

void CallLogWrapper::glVertexAttribI1ui (glw::GLuint index, glw::GLuint x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1ui(" << index << ", " << x << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1ui(index, x);
}

void CallLogWrapper::glVertexAttribI1uiv (glw::GLuint index, const glw::GLuint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1uiv(" << index << ", " << getPointerStr(v, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1uiv(index, v);
}

void CallLogWrapper::glVertexAttribI2i (glw::GLuint index, glw::GLint x, glw::GLint y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2i(" << index << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2i(index, x, y);
}

void CallLogWrapper::glVertexAttribI2iv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2iv(" << index << ", " << getPointerStr(v, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2iv(index, v);
}

void CallLogWrapper::glVertexAttribI2ui (glw::GLuint index, glw::GLuint x, glw::GLuint y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2ui(" << index << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2ui(index, x, y);
}

void CallLogWrapper::glVertexAttribI2uiv (glw::GLuint index, const glw::GLuint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2uiv(" << index << ", " << getPointerStr(v, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2uiv(index, v);
}

void CallLogWrapper::glVertexAttribI3i (glw::GLuint index, glw::GLint x, glw::GLint y, glw::GLint z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3i(" << index << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3i(index, x, y, z);
}

void CallLogWrapper::glVertexAttribI3iv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3iv(" << index << ", " << getPointerStr(v, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3iv(index, v);
}

void CallLogWrapper::glVertexAttribI3ui (glw::GLuint index, glw::GLuint x, glw::GLuint y, glw::GLuint z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3ui(" << index << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3ui(index, x, y, z);
}

void CallLogWrapper::glVertexAttribI3uiv (glw::GLuint index, const glw::GLuint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3uiv(" << index << ", " << getPointerStr(v, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3uiv(index, v);
}

void CallLogWrapper::glVertexAttribI4bv (glw::GLuint index, const glw::GLbyte *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4bv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4bv(index, v);
}

void CallLogWrapper::glVertexAttribI4i (glw::GLuint index, glw::GLint x, glw::GLint y, glw::GLint z, glw::GLint w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4i(" << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4i(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttribI4iv (glw::GLuint index, const glw::GLint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4iv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4iv(index, v);
}

void CallLogWrapper::glVertexAttribI4sv (glw::GLuint index, const glw::GLshort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4sv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4sv(index, v);
}

void CallLogWrapper::glVertexAttribI4ubv (glw::GLuint index, const glw::GLubyte *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4ubv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4ubv(index, v);
}

void CallLogWrapper::glVertexAttribI4ui (glw::GLuint index, glw::GLuint x, glw::GLuint y, glw::GLuint z, glw::GLuint w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4ui(" << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4ui(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttribI4uiv (glw::GLuint index, const glw::GLuint *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4uiv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4uiv(index, v);
}

void CallLogWrapper::glVertexAttribI4usv (glw::GLuint index, const glw::GLushort *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4usv(" << index << ", " << getPointerStr(v, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4usv(index, v);
}

void CallLogWrapper::glVertexAttribIFormat (glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribIFormat(" << attribindex << ", " << size << ", " << getTypeStr(type) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexAttribIFormat(attribindex, size, type, relativeoffset);
}

void CallLogWrapper::glVertexAttribIPointer (glw::GLuint index, glw::GLint size, glw::GLenum type, glw::GLsizei stride, const void *pointer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribIPointer(" << index << ", " << size << ", " << getTypeStr(type) << ", " << stride << ", " << pointer << ");" << TestLog::EndMessage;
	m_gl.vertexAttribIPointer(index, size, type, stride, pointer);
}

void CallLogWrapper::glVertexAttribL1d (glw::GLuint index, glw::GLdouble x)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL1d(" << index << ", " << x << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL1d(index, x);
}

void CallLogWrapper::glVertexAttribL1dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL1dv(" << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL1dv(index, v);
}

void CallLogWrapper::glVertexAttribL2d (glw::GLuint index, glw::GLdouble x, glw::GLdouble y)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL2d(" << index << ", " << x << ", " << y << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL2d(index, x, y);
}

void CallLogWrapper::glVertexAttribL2dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL2dv(" << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL2dv(index, v);
}

void CallLogWrapper::glVertexAttribL3d (glw::GLuint index, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL3d(" << index << ", " << x << ", " << y << ", " << z << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL3d(index, x, y, z);
}

void CallLogWrapper::glVertexAttribL3dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL3dv(" << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL3dv(index, v);
}

void CallLogWrapper::glVertexAttribL4d (glw::GLuint index, glw::GLdouble x, glw::GLdouble y, glw::GLdouble z, glw::GLdouble w)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL4d(" << index << ", " << x << ", " << y << ", " << z << ", " << w << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL4d(index, x, y, z, w);
}

void CallLogWrapper::glVertexAttribL4dv (glw::GLuint index, const glw::GLdouble *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL4dv(" << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL4dv(index, v);
}

void CallLogWrapper::glVertexAttribLFormat (glw::GLuint attribindex, glw::GLint size, glw::GLenum type, glw::GLuint relativeoffset)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribLFormat(" << attribindex << ", " << size << ", " << toHex(type) << ", " << relativeoffset << ");" << TestLog::EndMessage;
	m_gl.vertexAttribLFormat(attribindex, size, type, relativeoffset);
}

void CallLogWrapper::glVertexAttribLPointer (glw::GLuint index, glw::GLint size, glw::GLenum type, glw::GLsizei stride, const void *pointer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribLPointer(" << index << ", " << size << ", " << toHex(type) << ", " << stride << ", " << pointer << ");" << TestLog::EndMessage;
	m_gl.vertexAttribLPointer(index, size, type, stride, pointer);
}

void CallLogWrapper::glVertexAttribP1ui (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, glw::GLuint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP1ui(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << value << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP1ui(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP1uiv (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP1uiv(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP1uiv(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP2ui (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, glw::GLuint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP2ui(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << value << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP2ui(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP2uiv (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP2uiv(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP2uiv(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP3ui (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, glw::GLuint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP3ui(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << value << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP3ui(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP3uiv (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP3uiv(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP3uiv(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP4ui (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, glw::GLuint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP4ui(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << value << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP4ui(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribP4uiv (glw::GLuint index, glw::GLenum type, glw::GLboolean normalized, const glw::GLuint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP4uiv(" << index << ", " << toHex(type) << ", " << getBooleanStr(normalized) << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(value))) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP4uiv(index, type, normalized, value);
}

void CallLogWrapper::glVertexAttribPointer (glw::GLuint index, glw::GLint size, glw::GLenum type, glw::GLboolean normalized, glw::GLsizei stride, const void *pointer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribPointer(" << index << ", " << size << ", " << getTypeStr(type) << ", " << getBooleanStr(normalized) << ", " << stride << ", " << pointer << ");" << TestLog::EndMessage;
	m_gl.vertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void CallLogWrapper::glVertexBindingDivisor (glw::GLuint bindingindex, glw::GLuint divisor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexBindingDivisor(" << bindingindex << ", " << divisor << ");" << TestLog::EndMessage;
	m_gl.vertexBindingDivisor(bindingindex, divisor);
}

void CallLogWrapper::glViewport (glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewport(" << x << ", " << y << ", " << width << ", " << height << ");" << TestLog::EndMessage;
	m_gl.viewport(x, y, width, height);
}

void CallLogWrapper::glViewportArrayv (glw::GLuint first, glw::GLsizei count, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewportArrayv(" << first << ", " << count << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.viewportArrayv(first, count, v);
}

void CallLogWrapper::glViewportIndexedf (glw::GLuint index, glw::GLfloat x, glw::GLfloat y, glw::GLfloat w, glw::GLfloat h)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewportIndexedf(" << index << ", " << x << ", " << y << ", " << w << ", " << h << ");" << TestLog::EndMessage;
	m_gl.viewportIndexedf(index, x, y, w, h);
}

void CallLogWrapper::glViewportIndexedfv (glw::GLuint index, const glw::GLfloat *v)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewportIndexedfv(" << index << ", " << toHex(reinterpret_cast<uintptr_t>(static_cast<const void*>(v))) << ");" << TestLog::EndMessage;
	m_gl.viewportIndexedfv(index, v);
}

void CallLogWrapper::glWaitSync (glw::GLsync sync, glw::GLbitfield flags, glw::GLuint64 timeout)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glWaitSync(" << sync << ", " << toHex(flags) << ", " << timeout << ");" << TestLog::EndMessage;
	m_gl.waitSync(sync, flags, timeout);
}
