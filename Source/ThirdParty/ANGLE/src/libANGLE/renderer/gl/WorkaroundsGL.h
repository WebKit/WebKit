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
          alwaysCallUseProgramAfterLink(false),
          unpackOverlappingRowsSeparatelyUnpackBuffer(false),
          emulateAbsIntFunction(false),
          addAndTrueToLoopCondition(false),
          emulateIsnanFloat(false),
          useUnusedBlocksWithStandardOrSharedLayout(false)
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

    // In the case of unpacking from a pixel unpack buffer, unpack overlapping rows row by row.
    bool unpackOverlappingRowsSeparatelyUnpackBuffer;
    // In the case of packing to a pixel pack buffer, pack overlapping rows row by row.
    bool packOverlappingRowsSeparatelyPackBuffer;

    // During initialization, assign the current vertex attributes to the spec-mandated defaults.
    bool initializeCurrentVertexAttributes;

    // abs(i) where i is an integer returns unexpected result on Intel Mac.
    // Emulate abs(i) with i * sign(i).
    bool emulateAbsIntFunction;

    // On Intel Mac, calculation of loop conditions in for and while loop has bug.
    // Add "&& true" to the end of the condition expression to work around the bug.
    bool addAndTrueToLoopCondition;

    // When uploading textures from an unpack buffer, some drivers count an extra row padding when
    // checking if the pixel unpack buffer is big enough. Tracking bug: http://anglebug.com/1512
    // For example considering the pixel buffer below where in memory, each row data (D) of the
    // texture is followed by some unused data (the dots):
    //     +-------+--+
    //     |DDDDDDD|..|
    //     |DDDDDDD|..|
    //     |DDDDDDD|..|
    //     |DDDDDDD|..|
    //     +-------A--B
    // The last pixel read will be A, but the driver will think it is B, causing it to generate an
    // error when the pixel buffer is just big enough.
    bool unpackLastRowSeparatelyForPaddingInclusion;

    // Equivalent workaround when uploading data from a pixel pack buffer.
    bool packLastRowSeparatelyForPaddingInclusion;

    // On some Intel drivers, using isnan() on highp float will get wrong answer. To work around
    // this bug, we use an expression to emulate function isnan().
    // Tracking bug: http://crbug.com/650547
    bool emulateIsnanFloat;

    // On Mac with OpenGL version 4.1, unused std140 or shared uniform blocks will be
    // treated as inactive which is not consistent with WebGL2.0 spec. Reference all members in a
    // unused std140 or shared uniform block at the beginning of main to work around it.
    bool useUnusedBlocksWithStandardOrSharedLayout;
};
}

#endif  // LIBANGLE_RENDERER_GL_WORKAROUNDSGL_H_
