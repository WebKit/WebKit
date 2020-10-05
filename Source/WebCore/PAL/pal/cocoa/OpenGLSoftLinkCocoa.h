/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/SoftLinking.h>

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)

#include <OpenGL/OpenGL.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, OpenGL)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLChoosePixelFormat, CGLError, (const CGLPixelFormatAttribute *attribs, CGLPixelFormatObj *pix, GLint *npix), (attribs, pix, npix))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLCreateContext, CGLError, (CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx), (pix, share, ctx))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLDescribePixelFormat, CGLError, (CGLPixelFormatObj pix, GLint pix_num, CGLPixelFormatAttribute attrib, GLint *value), (pix, pix_num, attrib, value))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLDescribeRenderer, CGLError, (CGLRendererInfoObj rend, GLint rend_num, CGLRendererProperty prop, GLint *value), (rend, rend_num, prop, value))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLDestroyContext, CGLError, (CGLContextObj ctx), (ctx))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLDestroyRendererInfo, CGLError, (CGLRendererInfoObj rend), (rend))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLGetParameter, CGLError, (CGLContextObj ctx, CGLContextParameter pname, GLint *params), (ctx, pname, params))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLQueryRendererInfo, CGLError, (GLuint display_mask, CGLRendererInfoObj *rend, GLint *nrend), (display_mask, rend, nrend))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLReleasePixelFormat, void, (CGLPixelFormatObj pix), (pix))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLSetVirtualScreen, CGLError, (CGLContextObj ctx, GLint screen), (ctx, screen))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, OpenGL, CGLUpdateContext, CGLError, (CGLContextObj ctx), (ctx))

#endif
