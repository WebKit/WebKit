/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef GraphicsContext3DIOS_h
#define GraphicsContext3DIOS_h

#define glBindFramebufferEXT glBindFramebuffer
#define glBindRenderbufferEXT glBindRenderbuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define glDeleteRenderbuffersEXT glDeleteRenderbuffers
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glGenerateMipmapEXT glGenerateMipmap
#define glGenFramebuffersEXT glGenFramebuffers
#define glGenRenderbuffersEXT glGenRenderbuffers
#define glGetFramebufferAttachmentParameterivEXT glGetFramebufferAttachmentParameteriv
#define glGetRenderbufferParameterivEXT glGetRenderbufferParameteriv
#define glIsFramebufferEXT glIsFramebuffer
#define glIsRenderbufferEXT glIsRenderbuffer
#define glRenderbufferStorageEXT glRenderbufferStorage
#define glRenderbufferStorageMultisampleEXT glRenderbufferStorageMultisampleAPPLE

#define glDrawArraysInstancedARB glDrawArraysInstancedEXT
#define glDrawElementsInstancedARB glDrawElementsInstancedEXT
#define glVertexAttribDivisorARB glVertexAttribDivisorEXT

#define GL_COLOR_ATTACHMENT0_EXT GL_COLOR_ATTACHMENT0
#define GL_DEPTH24_STENCIL8_EXT GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH_ATTACHMENT_EXT  GL_DEPTH_ATTACHMENT
#define GL_DRAW_FRAMEBUFFER_EXT GL_DRAW_FRAMEBUFFER_APPLE
#define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_EXT GL_FRAMEBUFFER
#define GL_MAX_SAMPLES_EXT GL_MAX_SAMPLES_APPLE
#define GL_READ_FRAMEBUFFER_EXT GL_READ_FRAMEBUFFER_APPLE
#define GL_RENDERBUFFER_EXT GL_RENDERBUFFER
#ifndef GL_RGB8
#define GL_RGB8 GL_RGB
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 GL_RGBA
#endif
#define GL_STENCIL_ATTACHMENT_EXT GL_STENCIL_ATTACHMENT
#define GL_UNSIGNED_INT_8_8_8_8_REV GL_UNSIGNED_BYTE

#endif // GraphicsContext3DIOS_h
