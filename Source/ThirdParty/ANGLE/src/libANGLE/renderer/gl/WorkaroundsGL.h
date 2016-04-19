//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WorkaroundsGL.h: Workarounds for GL driver bugs and other issues.

#ifndef LIBANGLE_RENDERER_GL_WORKAROUNDSGL_H_
#define LIBANGLE_RENDERER_GL_WORKAROUNDSGL_H_

namespace rx
{

struct WorkaroundsGL
{
    WorkaroundsGL()
        : avoid1BitAlphaTextureFormats(false),
          rgba4IsNotSupportedForColorRendering(false),
          doesSRGBClearsOnLinearFramebufferAttachments(false),
          doWhileGLSLCausesGPUHang(false),
          finishDoesNotCauseQueriesToBeAvailable(false),
          alwaysCallUseProgramAfterLink(false)
    {
    }

    // When writing a float to a normalized integer framebuffer, desktop OpenGL is allowed to write
    // one of the two closest normalized integer representations (although round to nearest is
    // preferred) (see section 2.3.5.2 of the GL 4.5 core specification). OpenGL ES requires that
    // round-to-nearest is used (see "Conversion from Floating-Point to Framebuffer Fixed-Point" in
    // section 2.1.2 of the OpenGL ES 2.0.25 spec).  This issue only shows up on Intel and AMD
    // drivers on framebuffer formats that have 1-bit alpha, work around this by using higher
    // precision formats instead.
    bool avoid1BitAlphaTextureFormats;

    // On some older Intel drivers, GL_RGBA4 is not color renderable, glCheckFramebufferStatus
    // returns GL_FRAMEBUFFER_UNSUPPORTED. Work around this by using a known color-renderable
    // format.
    bool rgba4IsNotSupportedForColorRendering;

    // When clearing a framebuffer on Intel or AMD drivers, when GL_FRAMEBUFFER_SRGB is enabled, the
    // driver clears to the linearized clear color despite the framebuffer not supporting SRGB
    // blending.  It only seems to do this when the framebuffer has only linear attachments, mixed
    // attachments appear to get the correct clear color.
    bool doesSRGBClearsOnLinearFramebufferAttachments;

    // On Mac some GLSL constructs involving do-while loops cause GPU hangs, such as the following:
    //  int i = 1;
    //  do {
    //      i --;
    //      continue;
    //  } while (i > 0)
    // Work around this by rewriting the do-while to use another GLSL construct (block + while)
    bool doWhileGLSLCausesGPUHang;

    // Calling glFinish doesn't cause all queries to report that the result is available on some
    // (NVIDIA) drivers.  It was found that enabling GL_DEBUG_OUTPUT_SYNCHRONOUS before the finish
    // causes it to fully finish.
    bool finishDoesNotCauseQueriesToBeAvailable;

    // Always call useProgram after a successful link to avoid a driver bug.
    // This workaround is meant to reproduce the use_current_program_after_successful_link
    // workaround in Chromium (http://crbug.com/110263). It has been shown that this workaround is
    // not necessary for MacOSX 10.9 and higher (http://crrev.com/39eb535b).
    bool alwaysCallUseProgramAfterLink;
};
}

#endif  // LIBANGLE_RENDERER_GL_WORKAROUNDSGL_H_
