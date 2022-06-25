/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <epoxy/gl.h>

// Undefine the OpenGL EXT entrypoints and instead define them to the non-extension
// variants. This mirrors OpenGLESShims.h, but the un-definition has to be done first
// due to epoxy headers already being included.

// Unlike OpenGLESShims.h, we don't define specific constants since those are already
// provided by the libepoxy headers, and their values are the same regardless of the
// ARB, EXT or OES suffix.

#undef glBindFramebufferEXT
#define glBindFramebufferEXT glBindFramebuffer

#undef glFramebufferTexture2DEXT
#define glFramebufferTexture2DEXT glFramebufferTexture2D

#undef glBindRenderbufferEXT
#define glBindRenderbufferEXT glBindRenderbuffer

#undef glRenderbufferStorageEXT
#define glRenderbufferStorageEXT glRenderbufferStorage

#undef glFramebufferRenderbufferEXT
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer

#undef glCheckFramebufferStatusEXT
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus

#undef glDeleteFramebuffersEXT
#define glDeleteFramebuffersEXT glDeleteFramebuffers

#undef glDeleteRenderbuffersEXT
#define glDeleteRenderbuffersEXT glDeleteRenderbuffers

#undef glGenRenderbuffersEXT
#define glGenRenderbuffersEXT glGenRenderbuffers

#undef glGenFramebuffersEXT
#define glGenFramebuffersEXT glGenFramebuffers

#undef glGetFramebufferAttachmentParameterivEXT
#define glGetFramebufferAttachmentParameterivEXT glGetFramebufferAttachmentParameteriv

#undef glGetRenderbufferParameterivEXT
#define glGetRenderbufferParameterivEXT glGetRenderbufferParameteriv

#undef glIsRenderbufferEXT
#define glIsRenderbufferEXT glIsRenderbuffer

#undef glIsFramebufferEXT
#define glIsFramebufferEXT glIsFramebuffer

#undef glGenerateMipmapEXT
#define glGenerateMipmapEXT glGenerateMipmap
