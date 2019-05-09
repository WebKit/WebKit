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
    WorkaroundsGL();

    // When writing a float to a normalized integer framebuffer, desktop OpenGL is allowed to write
    // one of the two closest normalized integer representations (although round to nearest is
    // preferred) (see section 2.3.5.2 of the GL 4.5 core specification). OpenGL ES requires that
    // round-to-nearest is used (see "Conversion from Floating-Point to Framebuffer Fixed-Point" in
    // section 2.1.2 of the OpenGL ES 2.0.25 spec).  This issue only shows up on Intel and AMD
    // drivers on framebuffer formats that have 1-bit alpha, work around this by using higher
    // precision formats instead.
    bool avoid1BitAlphaTextureFormats = false;

    // On some older Intel drivers, GL_RGBA4 is not color renderable, glCheckFramebufferStatus
    // returns GL_FRAMEBUFFER_UNSUPPORTED. Work around this by using a known color-renderable
    // format.
    bool rgba4IsNotSupportedForColorRendering = false;

    // When clearing a framebuffer on Intel or AMD drivers, when GL_FRAMEBUFFER_SRGB is enabled, the
    // driver clears to the linearized clear color despite the framebuffer not supporting SRGB
    // blending.  It only seems to do this when the framebuffer has only linear attachments, mixed
    // attachments appear to get the correct clear color.
    bool doesSRGBClearsOnLinearFramebufferAttachments = false;

    // On Mac some GLSL constructs involving do-while loops cause GPU hangs, such as the following:
    //  int i = 1;
    //  do {
    //      i --;
    //      continue;
    //  } while (i > 0)
    // Work around this by rewriting the do-while to use another GLSL construct (block + while)
    bool doWhileGLSLCausesGPUHang = false;

    // Calling glFinish doesn't cause all queries to report that the result is available on some
    // (NVIDIA) drivers.  It was found that enabling GL_DEBUG_OUTPUT_SYNCHRONOUS before the finish
    // causes it to fully finish.
    bool finishDoesNotCauseQueriesToBeAvailable = false;

    // Always call useProgram after a successful link to avoid a driver bug.
    // This workaround is meant to reproduce the use_current_program_after_successful_link
    // workaround in Chromium (http://crbug.com/110263). It has been shown that this workaround is
    // not necessary for MacOSX 10.9 and higher (http://crrev.com/39eb535b).
    bool alwaysCallUseProgramAfterLink = false;

    // In the case of unpacking from a pixel unpack buffer, unpack overlapping rows row by row.
    bool unpackOverlappingRowsSeparatelyUnpackBuffer = false;
    // In the case of packing to a pixel pack buffer, pack overlapping rows row by row.
    bool packOverlappingRowsSeparatelyPackBuffer = false;

    // During initialization, assign the current vertex attributes to the spec-mandated defaults.
    bool initializeCurrentVertexAttributes = false;

    // abs(i) where i is an integer returns unexpected result on Intel Mac.
    // Emulate abs(i) with i * sign(i).
    bool emulateAbsIntFunction = false;

    // On Intel Mac, calculation of loop conditions in for and while loop has bug.
    // Add "&& true" to the end of the condition expression to work around the bug.
    bool addAndTrueToLoopCondition = false;

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
    bool unpackLastRowSeparatelyForPaddingInclusion = false;

    // Equivalent workaround when uploading data from a pixel pack buffer.
    bool packLastRowSeparatelyForPaddingInclusion = false;

    // On some Intel drivers, using isnan() on highp float will get wrong answer. To work around
    // this bug, we use an expression to emulate function isnan().
    // Tracking bug: http://crbug.com/650547
    bool emulateIsnanFloat = false;

    // On Mac with OpenGL version 4.1, unused std140 or shared uniform blocks will be
    // treated as inactive which is not consistent with WebGL2.0 spec. Reference all members in a
    // unused std140 or shared uniform block at the beginning of main to work around it.
    // Also used on Linux AMD.
    bool useUnusedBlocksWithStandardOrSharedLayout = false;

    // This flag will keep invariant declaration for input in fragment shader for GLSL >=4.20
    // on AMD.
    bool dontRemoveInvariantForFragmentInput = false;

    // This flag is used to fix spec difference between GLSL 4.1 or lower and ESSL3.
    bool removeInvariantAndCentroidForESSL3 = false;

    // On Intel Mac OSX 10.11 driver, using "-float" will get wrong answer. Use "0.0 - float" to
    // replace "-float".
    // Tracking bug: http://crbug.com/308366
    bool rewriteFloatUnaryMinusOperator = false;

    // On NVIDIA drivers, atan(y, x) may return a wrong answer.
    // Tracking bug: http://crbug.com/672380
    bool emulateAtan2Float = false;

    // Some drivers seem to forget about UBO bindings when using program binaries. Work around
    // this by re-applying the bindings after the program binary is loaded or saved.
    // This only seems to affect AMD OpenGL drivers, and some Android devices.
    // http://anglebug.com/1637
    bool reapplyUBOBindingsAfterUsingBinaryProgram = false;

    // Some OpenGL drivers return 0 when we query MAX_VERTEX_ATTRIB_STRIDE in an OpenGL 4.4 or
    // higher context.
    // This only seems to affect AMD OpenGL drivers.
    // Tracking bug: http://anglebug.com/1936
    bool emulateMaxVertexAttribStride = false;

    // Initializing uninitialized locals caused odd behavior on Mac in a few WebGL 2 tests.
    // Tracking bug: http://anglebug/2041
    bool dontInitializeUninitializedLocals = false;

    // On some NVIDIA drivers the point size range reported from the API is inconsistent with the
    // actual behavior. Clamp the point size to the value from the API to fix this.
    bool clampPointSize = false;

    // On some NVIDIA drivers certain types of GLSL arithmetic ops mixing vectors and scalars may be
    // executed incorrectly. Change them in the shader translator. Tracking bug:
    // http://crbug.com/772651
    bool rewriteVectorScalarArithmetic = false;

    // On some Android devices for loops used to initialize variables hit native GLSL compiler bugs.
    bool dontUseLoopsToInitializeVariables = false;

    // On some NVIDIA drivers gl_FragDepth is not clamped correctly when rendering to a floating
    // point depth buffer. Clamp it in the translated shader to fix this.
    bool clampFragDepth = false;

    // On some NVIDIA drivers before version 397.31 repeated assignment to swizzled values inside a
    // GLSL user-defined function have incorrect results. Rewrite this type of statements to fix
    // this.
    bool rewriteRepeatedAssignToSwizzled = false;

    // On some AMD and Intel GL drivers ARB_blend_func_extended does not pass the tests.
    // It might be possible to work around the Intel bug by rewriting *FragData to *FragColor
    // instead of disabling the functionality entirely. The AMD bug looked like incorrect blending,
    // not sure if a workaround is feasible. http://anglebug.com/1085
    bool disableBlendFuncExtended = false;

    // Qualcomm drivers returns raw sRGB values instead of linearized values when calling
    // glReadPixels on unsized sRGB texture formats. http://crbug.com/550292 and
    // http://crbug.com/565179
    bool unsizedsRGBReadPixelsDoesntTransform = false;

    // Older Qualcomm drivers generate errors when querying the number of bits in timer queries, ex:
    // GetQueryivEXT(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS).  http://anglebug.com/3027
    bool queryCounterBitsGeneratesErrors = false;

    // Re-linking a program in parallel is buggy on some Intel Windows OpenGL drivers and Android
    // platforms.
    // http://anglebug.com/3045
    bool dontRelinkProgramsInParallel = false;

    // Some tests have been seen to fail using worker contexts, this switch allows worker contexts
    // to be disabled for some platforms. http://crbug.com/849576
    bool disableWorkerContexts = false;
};

inline WorkaroundsGL::WorkaroundsGL() = default;

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_WORKAROUNDSGL_H_
