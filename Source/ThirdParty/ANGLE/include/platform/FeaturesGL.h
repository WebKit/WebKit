//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FeaturesGL.h: Features and workarounds for GL driver bugs and other issues.

#ifndef ANGLE_PLATFORM_FEATURESGL_H_
#define ANGLE_PLATFORM_FEATURESGL_H_

#include "platform/Feature.h"

namespace angle
{

struct FeaturesGL : FeatureSetBase
{
    FeaturesGL();
    ~FeaturesGL();

    // When writing a float to a normalized integer framebuffer, desktop OpenGL is allowed to write
    // one of the two closest normalized integer representations (although round to nearest is
    // preferred) (see section 2.3.5.2 of the GL 4.5 core specification). OpenGL ES requires that
    // round-to-nearest is used (see "Conversion from Floating-Point to Framebuffer Fixed-Point" in
    // section 2.1.2 of the OpenGL ES 2.0.25 spec).  This issue only shows up on AMD drivers on
    // framebuffer formats that have 1-bit alpha, work around this by using higher precision formats
    // instead.
    Feature avoid1BitAlphaTextureFormats = {"avoid_1_bit_alpha_texture_formats",
                                            FeatureCategory::OpenGLWorkarounds,
                                            "Issue with 1-bit alpha framebuffer formats", &members};

    // On some older Intel drivers, GL_RGBA4 is not color renderable, glCheckFramebufferStatus
    // returns GL_FRAMEBUFFER_UNSUPPORTED. Work around this by using a known color-renderable
    // format.
    Feature rgba4IsNotSupportedForColorRendering = {"rgba4_is_not_supported_for_color_rendering",
                                                    FeatureCategory::OpenGLWorkarounds,
                                                    "GL_RGBA4 is not color renderable", &members};

    // Newer Intel GPUs natively support ETC2/EAC compressed texture formats.
    Feature allowEtcFormats = {"allow_etc_formats", FeatureCategory::OpenGLWorkarounds,
                               "Enable ETC2/EAC on desktop OpenGL", &members};

    // When clearing a framebuffer on Intel or AMD drivers, when GL_FRAMEBUFFER_SRGB is enabled, the
    // driver clears to the linearized clear color despite the framebuffer not supporting SRGB
    // blending.  It only seems to do this when the framebuffer has only linear attachments, mixed
    // attachments appear to get the correct clear color.
    Feature doesSRGBClearsOnLinearFramebufferAttachments = {
        "does_srgb_clears_on_linear_framebuffer_attachments", FeatureCategory::OpenGLWorkarounds,
        "Issue clearing framebuffers with linear attachments when GL_FRAMEBUFFER_SRGB is enabled",
        &members};

    // On Mac some GLSL constructs involving do-while loops cause GPU hangs, such as the following:
    //  int i = 1;
    //  do {
    //      i --;
    //      continue;
    //  } while (i > 0)
    // Work around this by rewriting the do-while to use another GLSL construct (block + while)
    Feature doWhileGLSLCausesGPUHang = {
        "do_while_glsl_causes_gpu_hang", FeatureCategory::OpenGLWorkarounds,
        "Some GLSL constructs involving do-while loops cause GPU hangs", &members,
        "http://crbug.com/644669"};

    // On Mac AMD GPU gl_VertexID in GLSL vertex shader doesn't include base vertex value,
    // Work aronud this by replace gl_VertexID with (gl_VertexID - angle_BaseVertex) when
    // angle_BaseVertex is present.
    Feature addBaseVertexToVertexID = {
        "vertex_id_does_not_include_base_vertex", FeatureCategory::OpenGLWorkarounds,
        "gl_VertexID in GLSL vertex shader doesn't include base vertex value", &members};

    // Calling glFinish doesn't cause all queries to report that the result is available on some
    // (NVIDIA) drivers.  It was found that enabling GL_DEBUG_OUTPUT_SYNCHRONOUS before the finish
    // causes it to fully finish.
    Feature finishDoesNotCauseQueriesToBeAvailable = {
        "finish_does_not_cause_queries_to_be_available", FeatureCategory::OpenGLWorkarounds,
        "glFinish doesn't cause all queries to report available result", &members};

    // Always call useProgram after a successful link to avoid a driver bug.
    // This workaround is meant to reproduce the use_current_program_after_successful_link
    // workaround in Chromium (http://crbug.com/110263). It has been shown that this workaround is
    // not necessary for MacOSX 10.9 and higher (http://crrev.com/39eb535b).
    Feature alwaysCallUseProgramAfterLink = {
        "always_call_use_program_after_link", FeatureCategory::OpenGLWorkarounds,
        "Always call useProgram after a successful link to avoid a driver bug", &members,
        "http://crbug.com/110263"};

    // On NVIDIA, in the case of unpacking from a pixel unpack buffer, unpack overlapping rows row
    // by row.
    Feature unpackOverlappingRowsSeparatelyUnpackBuffer = {
        "unpack_overlapping_rows_separately_unpack_buffer", FeatureCategory::OpenGLWorkarounds,
        "In the case of unpacking from a pixel unpack buffer, unpack overlapping rows row by row",
        &members};

    // On NVIDIA, in the case of packing to a pixel pack buffer, pack overlapping rows row by row.
    Feature packOverlappingRowsSeparatelyPackBuffer = {
        "pack_overlapping_rows_separately_pack_buffer", FeatureCategory::OpenGLWorkarounds,
        "In the case of packing to a pixel pack buffer, pack overlapping rows row by row",
        &members};

    // On NVIDIA, during initialization, assign the current vertex attributes to the spec-mandated
    // defaults.
    Feature initializeCurrentVertexAttributes = {
        "initialize_current_vertex_attributes", FeatureCategory::OpenGLWorkarounds,
        "During initialization, assign the current vertex attributes to the spec-mandated defaults",
        &members};

    // abs(i) where i is an integer returns unexpected result on Intel Mac.
    // Emulate abs(i) with i * sign(i).
    Feature emulateAbsIntFunction = {"emulate_abs_int_function", FeatureCategory::OpenGLWorkarounds,
                                     "abs(i) where i is an integer returns unexpected result",
                                     &members, "http://crbug.com/642227"};

    // On Intel Mac, calculation of loop conditions in for and while loop has bug.
    // Add "&& true" to the end of the condition expression to work around the bug.
    Feature addAndTrueToLoopCondition = {
        "add_and_true_to_loop_condition", FeatureCategory::OpenGLWorkarounds,
        "Calculation of loop conditions in for and while loop has bug", &members};

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
    Feature unpackLastRowSeparatelyForPaddingInclusion = {
        "unpack_last_row_separately_for_padding_inclusion", FeatureCategory::OpenGLWorkarounds,
        "When uploading textures from an unpack buffer, some drivers count an extra row padding",
        &members, "http://anglebug.com/1512"};

    // Equivalent workaround when uploading data from a pixel pack buffer.
    Feature packLastRowSeparatelyForPaddingInclusion = {
        "pack_last_row_separately_for_padding_inclusion", FeatureCategory::OpenGLWorkarounds,
        "When uploading textures from an pack buffer, some drivers count an extra row padding",
        &members, "http://anglebug.com/1512"};

    // On some Intel drivers, using isnan() on highp float will get wrong answer. To work around
    // this bug, we use an expression to emulate function isnan().
    // Tracking bug: http://crbug.com/650547
    Feature emulateIsnanFloat = {"emulate_isnan_float", FeatureCategory::OpenGLWorkarounds,
                                 "Using isnan() on highp float will get wrong answer", &members,
                                 "http://crbug.com/650547"};

    // On Mac with OpenGL version 4.1, unused std140 or shared uniform blocks will be
    // treated as inactive which is not consistent with WebGL2.0 spec. Reference all members in a
    // unused std140 or shared uniform block at the beginning of main to work around it.
    // Also used on Linux AMD.
    Feature useUnusedBlocksWithStandardOrSharedLayout = {
        "use_unused_blocks_with_standard_or_shared_layout", FeatureCategory::OpenGLWorkarounds,
        "Unused std140 or shared uniform blocks will be treated as inactive", &members};

    // This flag is used to fix spec difference between GLSL 4.1 or lower and ESSL3.
    Feature removeInvariantAndCentroidForESSL3 = {
        "remove_invarient_and_centroid_for_essl3", FeatureCategory::OpenGLWorkarounds,
        "Fix spec difference between GLSL 4.1 or lower and ESSL3", &members};

    // On Intel Mac OSX 10.11 driver, using "-float" will get wrong answer. Use "0.0 - float" to
    // replace "-float".
    // Tracking bug: http://crbug.com/308366
    Feature rewriteFloatUnaryMinusOperator = {
        "rewrite_float_unary_minus_operator", FeatureCategory::OpenGLWorkarounds,
        "Using '-<float>' will get wrong answer", &members, "http://crbug.com/308366"};

    // On NVIDIA drivers, atan(y, x) may return a wrong answer.
    // Tracking bug: http://crbug.com/672380
    Feature emulateAtan2Float = {"emulate_atan_2_float", FeatureCategory::OpenGLWorkarounds,
                                 "atan(y, x) may return a wrong answer", &members,
                                 "http://crbug.com/672380"};

    // Some drivers seem to forget about UBO bindings when using program binaries. Work around
    // this by re-applying the bindings after the program binary is loaded or saved.
    // This only seems to affect AMD OpenGL drivers, and some Android devices.
    // http://anglebug.com/1637
    Feature reapplyUBOBindingsAfterUsingBinaryProgram = {
        "reapply_ubo_bindings_after_using_binary_program", FeatureCategory::OpenGLWorkarounds,
        "Some drivers forget about UBO bindings when using program binaries", &members,
        "http://anglebug.com/1637"};

    // Some Linux OpenGL drivers return 0 when we query MAX_VERTEX_ATTRIB_STRIDE in an OpenGL 4.4 or
    // higher context.
    // This only seems to affect AMD OpenGL drivers.
    // Tracking bug: http://anglebug.com/1936
    Feature emulateMaxVertexAttribStride = {
        "emulate_max_vertex_attrib_stride", FeatureCategory::OpenGLWorkarounds,
        "Some drivers return 0 when MAX_VERTEX_ATTRIB_STRIED queried", &members,
        "http://anglebug.com/1936"};

    // Initializing uninitialized locals caused odd behavior on Android Qualcomm in a few WebGL 2
    // tests. Tracking bug: http://anglebug.com/2046
    Feature dontInitializeUninitializedLocals = {
        "dont_initialize_uninitialized_locals", FeatureCategory::OpenGLWorkarounds,
        "Initializing uninitialized locals caused odd behavior in a few WebGL 2 tests", &members,
        "http://anglebug.com/2046"};

    // On some NVIDIA drivers the point size range reported from the API is inconsistent with the
    // actual behavior. Clamp the point size to the value from the API to fix this.
    Feature clampPointSize = {
        "clamp_point_size", FeatureCategory::OpenGLWorkarounds,
        "The point size range reported from the API is inconsistent with the actual behavior",
        &members};

    // On some Android devices for loops used to initialize variables hit native GLSL compiler bugs.
    Feature dontUseLoopsToInitializeVariables = {
        "dont_use_loops_to_initialize_variables", FeatureCategory::OpenGLWorkarounds,
        "For loops used to initialize variables hit native GLSL compiler bugs", &members,
        "http://crbug.com/809422"};

    // On some NVIDIA drivers gl_FragDepth is not clamped correctly when rendering to a floating
    // point depth buffer. Clamp it in the translated shader to fix this.
    Feature clampFragDepth = {
        "clamp_frag_depth", FeatureCategory::OpenGLWorkarounds,
        "gl_FragDepth is not clamped correctly when rendering to a floating point depth buffer",
        &members};

    // On some NVIDIA drivers before version 397.31 repeated assignment to swizzled values inside a
    // GLSL user-defined function have incorrect results. Rewrite this type of statements to fix
    // this.
    Feature rewriteRepeatedAssignToSwizzled = {"rewrite_repeated_assign_to_swizzled",
                                               FeatureCategory::OpenGLWorkarounds,
                                               "Repeated assignment to swizzled values inside a "
                                               "GLSL user-defined function have incorrect results",
                                               &members};

    // On some AMD and Intel GL drivers ARB_blend_func_extended does not pass the tests.
    // It might be possible to work around the Intel bug by rewriting *FragData to *FragColor
    // instead of disabling the functionality entirely. The AMD bug looked like incorrect blending,
    // not sure if a workaround is feasible. http://anglebug.com/1085
    Feature disableBlendFuncExtended = {
        "disable_blend_func_extended", FeatureCategory::OpenGLWorkarounds,
        "ARB_blend_func_extended does not pass the tests", &members, "http://anglebug.com/1085"};

    // Qualcomm drivers returns raw sRGB values instead of linearized values when calling
    // glReadPixels on unsized sRGB texture formats. http://crbug.com/550292 and
    // http://crbug.com/565179
    Feature unsizedsRGBReadPixelsDoesntTransform = {
        "unsized_srgb_read_pixels_doesnt_transform", FeatureCategory::OpenGLWorkarounds,
        "Drivers returning raw sRGB values instead of linearized values when calling glReadPixels "
        "on unsized sRGB texture formats",
        &members, "http://crbug.com/565179"};

    // Older Qualcomm drivers generate errors when querying the number of bits in timer queries, ex:
    // GetQueryivEXT(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS).  http://anglebug.com/3027
    Feature queryCounterBitsGeneratesErrors = {
        "query_counter_bits_generates_errors", FeatureCategory::OpenGLWorkarounds,
        "Drivers generate errors when querying the number of bits in timer queries", &members,
        "http://anglebug.com/3027"};

    // Re-linking a program in parallel is buggy on some Intel Windows OpenGL drivers and Android
    // platforms.
    // http://anglebug.com/3045
    Feature dontRelinkProgramsInParallel = {
        "dont_relink_programs_in_parallel", FeatureCategory::OpenGLWorkarounds,
        "Relinking a program in parallel is buggy", &members, "http://anglebug.com/3045"};

    // Some tests have been seen to fail using worker contexts, this switch allows worker contexts
    // to be disabled for some platforms. http://crbug.com/849576
    Feature disableWorkerContexts = {"disable_worker_contexts", FeatureCategory::OpenGLWorkarounds,
                                     "Some tests have been seen to fail using worker contexts",
                                     &members, "http://crbug.com/849576"};

    // Most Android devices fail to allocate a texture that is larger than 4096. Limit the caps
    // instead of generating GL_OUT_OF_MEMORY errors. Also causes system to hang on some older
    // intel mesa drivers on Linux.
    Feature limitMaxTextureSizeTo4096 = {"max_texture_size_limit_4096",
                                         FeatureCategory::OpenGLWorkarounds,
                                         "Limit max texture size to 4096 to avoid frequent "
                                         "out-of-memory errors",
                                         &members, "http://crbug.com/927470"};

    // Prevent excessive MSAA allocations on Android devices, various rendering bugs have been
    // observed and they tend to be high DPI anyways. http://crbug.com/797243
    Feature limitMaxMSAASamplesTo4 = {
        "max_msaa_sample_count_4", FeatureCategory::OpenGLWorkarounds,
        "Various rendering bugs have been observed when using higher MSAA counts", &members,
        "http://crbug.com/797243"};

    // Prefer to do the robust resource init clear using a glClear. Calls to TexSubImage2D on large
    // textures can take hundreds of milliseconds because of slow uploads on macOS. Do this only on
    // macOS because clears are buggy on other drivers.
    // https://crbug.com/848952 (slow uploads on macOS)
    // https://crbug.com/883276 (buggy clears on Android)
    Feature allowClearForRobustResourceInit = {
        "allow_clear_for_robust_resource_init", FeatureCategory::OpenGLWorkarounds,
        "Using glClear for robust resource initialization is buggy on some drivers and leads to "
        "texture corruption. Default to data uploads except on MacOS where it is very slow.",
        &members, "http://crbug.com/883276"};

    // Some drivers automatically handle out-of-bounds uniform array access but others need manual
    // clamping to satisfy the WebGL requirements.
    Feature clampArrayAccess = {"clamp_array_access", FeatureCategory::OpenGLWorkarounds,
                                "Clamp uniform array access to avoid reading invalid memory.",
                                &members, "http://anglebug.com/2978"};

    // Reset glTexImage2D base level to workaround pixel comparison failure above Mac OS 10.12.4 on
    // Intel Mac.
    Feature resetTexImage2DBaseLevel = {"reset_teximage2d_base_level",
                                        FeatureCategory::OpenGLWorkarounds,
                                        "Reset texture base level before calling glTexImage2D to "
                                        "work around pixel comparison failure.",
                                        &members, "https://crbug.com/705865"};

    // glClearColor does not always work on Intel 6xxx Mac drivers when the clear color made up of
    // all zeros and ones.
    Feature clearToZeroOrOneBroken = {
        "clear_to_zero_or_one_broken", FeatureCategory::OpenGLWorkarounds,
        "Clears when the clear color is all zeros or ones do not work.", &members,
        "https://crbug.com/710443"};

    // Some older Linux Intel mesa drivers will hang the system when allocating large textures. Fix
    // this by capping the max texture size.
    Feature limitMax3dArrayTextureSizeTo1024 = {
        "max_3d_array_texture_size_1024", FeatureCategory::OpenGLWorkarounds,
        "Limit max 3d texture size and max array texture layers to 1024 to avoid system hang",
        &members, "http://crbug.com/927470"};

    // BlitFramebuffer has issues on some platforms with large source/dest texture sizes. This
    // workaround adjusts the destination rectangle source and dest rectangle to fit within maximum
    // twice the size of the framebuffer.
    Feature adjustSrcDstRegionBlitFramebuffer = {
        "adjust_src_dst_region_for_blitframebuffer", FeatureCategory::OpenGLWorkarounds,
        "Many platforms have issues with blitFramebuffer when the parameters are large.", &members,
        "http://crbug.com/830046"};

    // BlitFramebuffer has issues on Mac when the source bounds aren't enclosed by the framebuffer.
    // This workaround clips the source region and adjust the dest region proportionally.
    Feature clipSrcRegionBlitFramebuffer = {
        "clip_src_region_for_blitframebuffer", FeatureCategory::OpenGLWorkarounds,
        "Issues with blitFramebuffer when the parameters don't match the framebuffer size.",
        &members, "http://crbug.com/830046"};

    // Mac Intel samples transparent black from GL_COMPRESSED_RGB_S3TC_DXT1_EXT
    Feature rgbDXT1TexturesSampleZeroAlpha = {
        "rgb_dxt1_textures_sample_zero_alpha", FeatureCategory::OpenGLWorkarounds,
        "Sampling BLACK texels from RGB DXT1 textures returns transparent black on Mac.", &members,
        "http://anglebug.com/3729"};

    // Mac incorrectly executes both sides of && and || expressions when they should short-circuit.
    Feature unfoldShortCircuits = {
        "unfold_short_circuits", FeatureCategory::OpenGLWorkarounds,
        "Mac incorrectly executes both sides of && and || expressions when they should "
        "short-circuit.",
        &members, "http://anglebug.com/482"};

    Feature emulatePrimitiveRestartFixedIndex = {
        "emulate_primitive_restart_fixed_index", FeatureCategory::OpenGLWorkarounds,
        "When GL_PRIMITIVE_RESTART_FIXED_INDEX is not available, emulate it with "
        "GL_PRIMITIVE_RESTART and glPrimitiveRestartIndex.",
        &members, "http://anglebug.com/3997"};

    Feature setPrimitiveRestartFixedIndexForDrawArrays = {
        "set_primitive_restart_fixed_index_for_draw_arrays", FeatureCategory::OpenGLWorkarounds,
        "Some drivers discard vertex data in DrawArrays calls when the fixed primitive restart "
        "index is within the number of primitives being drawn.",
        &members, "http://anglebug.com/3997"};

    // Dynamic indexing of swizzled l-values doesn't work correctly on various platforms.
    Feature removeDynamicIndexingOfSwizzledVector = {
        "remove_dynamic_indexing_of_swizzled_vector", FeatureCategory::OpenGLWorkarounds,
        "Dynamic indexing of swizzled l-values doesn't work correctly on various platforms.",
        &members, "http://crbug.com/709351"};

    // Intel Mac drivers does not treat texelFetchOffset() correctly.
    Feature preAddTexelFetchOffsets = {
        "pre_add_texel_fetch_offsets", FeatureCategory::OpenGLWorkarounds,
        "Intel Mac drivers mistakenly consider the parameter position of nagative vaule as invalid "
        "even if the sum of position and offset is in range, so we need to add workarounds by "
        "rewriting texelFetchOffset(sampler, position, lod, offset) into texelFetch(sampler, "
        "position + offset, lod).",
        &members, "http://crbug.com/642605"};

    // All Mac drivers do not handle struct scopes correctly. This workaround overwrites a struct
    // name with a unique prefix
    Feature regenerateStructNames = {
        "regenerate_struct_names", FeatureCategory::OpenGLWorkarounds,
        "All Mac drivers do not handle struct scopes correctly. This workaround overwrites a struct"
        "name with a unique prefix.",
        &members, "http://crbug.com/403957"};

    // Quite some OpenGL ES drivers don't implement readPixels for RGBA/UNSIGNED_SHORT from
    // EXT_texture_norm16 correctly
    Feature readPixelsUsingImplementationColorReadFormatForNorm16 = {
        "read_pixels_using_implementation_color_read_format", FeatureCategory::OpenGLWorkarounds,
        "Quite some OpenGL ES drivers don't implement readPixels for RGBA/UNSIGNED_SHORT from "
        "EXT_texture_norm16 correctly",
        &members, "http://anglebug.com/4214"};

    // Bugs exist in some Intel drivers where dependencies are incorrectly
    // tracked for textures which are copy destinations (via CopyTexImage2D, for
    // example). Flush before DeleteTexture if these entry points have been
    // called recently.
    Feature flushBeforeDeleteTextureIfCopiedTo = {
        "flush_before_delete_texture_if_copied_to", FeatureCategory::OpenGLWorkarounds,
        "Some drivers track CopyTex{Sub}Image texture dependencies incorrectly. Flush"
        " before glDeleteTextures in this case",
        &members, "http://anglebug.com/4267"};

    // Rewrite row-major matrices as column-major as a driver bug workaround if
    // necessary.
    Feature rewriteRowMajorMatrices = {
        "rewrite_row_major_matrices", FeatureCategory::OpenGLWorkarounds,
        "Rewrite row major matrices in shaders as column major as a driver bug workaround",
        &members, "http://anglebug.com/2273"};

    // Bugs exist in OpenGL AMD drivers on Windows that produce incorrect pipeline state for
    // colorMaski calls.
    Feature disableDrawBuffersIndexed = {"disable_draw_buffers_indexed",
                                         FeatureCategory::OpenGLWorkarounds,
                                         "Disable OES_draw_buffers_indexed extension.", &members};

    // GL_EXT_semaphore_fd doesn't work properly with Mesa 19.3.4 and earlier versions.
    Feature disableSemaphoreFd = {"disable_semaphore_fd", FeatureCategory::OpenGLWorkarounds,
                                  "Disable GL_EXT_semaphore_fd extension", &members,
                                  "https://crbug.com/1046462"};

    // GL_EXT_disjoint_timer_query doesn't work properly with Linux VMWare drivers.
    Feature disableTimestampQueries = {
        "disable_timestamp_queries", FeatureCategory::OpenGLWorkarounds,
        "Disable GL_EXT_disjoint_timer_query extension", &members, "https://crbug.com/811661"};

    // Some drivers use linear blending when generating mipmaps for sRGB textures. Work around this
    // by generating mipmaps in a linear texture and copying back to sRGB.
    Feature encodeAndDecodeSRGBForGenerateMipmap = {
        "decode_encode_srgb_for_generatemipmap", FeatureCategory::OpenGLWorkarounds,
        "Decode and encode before generateMipmap for srgb format textures.", &members,
        "http://anglebug.com/4646"};

    Feature emulateCopyTexImage2DFromRenderbuffers = {
        "emulate_copyteximage2d_from_renderbuffers", FeatureCategory::OpenGLWorkarounds,
        "CopyTexImage2D spuriously returns errors on iOS when copying from renderbuffers.",
        &members, "https://anglebug.com/4674"};

    Feature disableGPUSwitchingSupport = {
        "disable_gpu_switching_support", FeatureCategory::OpenGLWorkarounds,
        "Disable GPU switching support (use only the low-power GPU) on older MacBook Pros.",
        &members, "https://crbug.com/1091824"};

    // KHR_parallel_shader_compile fails TSAN on Linux, so we avoid using it with this workaround.
    Feature disableNativeParallelCompile = {
        "disable_native_parallel_compile", FeatureCategory::OpenGLWorkarounds,
        "Do not use native KHR_parallel_shader_compile even when available.", &members,
        "http://crbug.com/1094869"};

    Feature emulatePackSkipRowsAndPackSkipPixels = {
        "emulate_pack_skip_rows_and_pack_skip_pixels", FeatureCategory::OpenGLWorkarounds,
        "GL_PACK_SKIP_ROWS and GL_PACK_SKIP_PIXELS are ignored in Apple's OpenGL driver.", &members,
        "https://anglebug.com/4849"};

    // Some drivers return bogus/1hz values for GetMscRate, which we may want to clamp
    Feature clampMscRate = {
        "clamp_msc_rate", FeatureCategory::OpenGLWorkarounds,
        "Some drivers return bogus values for GetMscRate, so we clamp it to 30Hz", &members,
        "https://crbug.com/1042393"};

    // Mac drivers generate GL_INVALID_VALUE when binding a transform feedback buffer with
    // glBindBufferRange before first binding it to some generic binding point.
    Feature bindTransformFeedbackBufferBeforeBindBufferRange = {
        "bind_transform_feedback_buffer_before_bind_buffer_range",
        FeatureCategory::OpenGLWorkarounds,
        "Bind transform feedback buffers to the generic binding point before calling "
        "glBindBufferBase or glBindBufferRange.",
        &members, "https://anglebug.com/5140"};

    // Speculative fix for issues on Linux/Wayland where exposing GLX_OML_sync_control renders
    // Chrome unusable
    Feature disableSyncControlSupport = {
        "disable_sync_control_support", FeatureCategory::OpenGLWorkarounds,
        "Speculative fix for issues on Linux/Wayland where exposing GLX_OML_sync_control renders "
        "Chrome unusable",
        &members, "https://crbug.com/1137851"};

    // Buffers need to maintain a shadow copy of data when buffer data readback is not possible
    // through the GL API
    Feature keepBufferShadowCopy = {
        "keep_buffer_shadow_copy", FeatureCategory::OpenGLWorkarounds,
        "Maintain a shadow copy of buffer data when the GL API does not permit reading data back.",
        &members};

    // glGenerateMipmap fails if the zero texture level is not set on some Mac drivers
    Feature setZeroLevelBeforeGenerateMipmap = {
        "set_zero_level_before_generating_mipmap", FeatureCategory::OpenGLWorkarounds,
        "glGenerateMipmap fails if the zero texture level is not set on some Mac drivers.",
        &members};

    // On macOS with AMD GPUs, packed color formats like RGB565 and RGBA4444 are buggy. Promote them
    // to 8 bit per channel formats.
    Feature promotePackedFormatsTo8BitPerChannel = {
        "promote_packed_formats_to_8_bit_per_channel", FeatureCategory::OpenGLWorkarounds,
        "Packed color formats are buggy on Macs with AMD GPUs", &members,
        "http://anglebug.com/5469"};

    // If gl_FragColor is not written by fragment shader, it may cause context lost with Adreno 42x
    // and 3xx.
    Feature initFragmentOutputVariables = {
        "init_fragment_output_variables", FeatureCategory::OpenGLWorkarounds,
        "No init gl_FragColor causes context lost", &members, "http://crbug.com/1171371"};

    // On macOS with Intel GPUs, instanced array with divisor > 0 is buggy when first > 0 in
    // drawArraysInstanced. Shift the attributes with extra offset to workaround.
    Feature shiftInstancedArrayDataWithExtraOffset = {
        "shift_instanced_array_data_with_offset", FeatureCategory::OpenGLWorkarounds,
        "glDrawArraysInstanced is buggy on certain new Mac Intel GPUs", &members,
        "http://crbug.com/1144207"};

    // ANGLE needs to support devices that have no native VAOs. Sync everything to the default VAO.
    Feature syncVertexArraysToDefault = {
        "sync_vertex_arrays_to_default", FeatureCategory::OpenGLWorkarounds,
        "Only use the default VAO because of missing support or driver bugs", &members,
        "http://anglebug.com/5577"};

    // On desktop Linux/AMD when using the amdgpu drivers, the precise kernel and DRM version are
    // leaked via GL_RENDERER. We workaround this to improve user privacy.
    Feature sanitizeAmdGpuRendererString = {
        "sanitize_amdgpu_renderer_string", FeatureCategory::OpenGLWorkarounds,
        "Strip precise kernel and DRM version information from amdgpu renderer strings.", &members,
        "http://crbug.com/1181193"};

    // Imagination GL drivers are buggy with context switching. We need to ubind fbo to workaround a
    // crash in the driver.
    Feature unbindFBOOnContextSwitch = {"unbind_fbo_before_switching_context",
                                        FeatureCategory::OpenGLWorkarounds,
                                        "Imagination GL drivers are buggy with context switching.",
                                        &members, "http://crbug.com/1181193"};

    Feature flushOnFramebufferChange = {"flush_on_framebuffer_change",
                                        FeatureCategory::OpenGLWorkarounds,
                                        "Switching framebuffers without a flush can lead to "
                                        "crashes on Intel 9th Generation GPU Macs.",
                                        &members, "http://crbug.com/1181068"};

    Feature disableMultisampledRenderToTexture = {
        "disable_mutlisampled_render_to_texture", FeatureCategory::OpenGLWorkarounds,
        "Many drivers have bugs when using GL_EXT_multisampled_render_to_texture", &members,
        "http://anglebug.com/2894"};

    // Mac OpenGL drivers often hang when calling glTexSubImage with >120kb of data. Instead, upload
    // the data in <120kb chunks.
    static constexpr const size_t kUploadTextureDataInChunksUploadSize = (120 * 1024) - 1;
    Feature uploadTextureDataInChunks                                  = {
        "chunked_texture_upload", FeatureCategory::OpenGLWorkarounds,
        "Upload texture data in <120kb chunks to work around Mac driver hangs and crashes.",
        &members, "http://crbug.com/1181068"};

    // Qualcomm drivers may sometimes reject immutable ASTC sliced 3D texture
    // allocation. Instead, use non-immutable allocation internally.
    Feature emulateImmutableCompressedTexture3D = {
        "emulate_immutable_compressed_texture_3d", FeatureCategory::OpenGLWorkarounds,
        "Use non-immutable texture allocation to work around a driver bug.", &members,
        "https://crbug.com/1060012"};
};

inline FeaturesGL::FeaturesGL()  = default;
inline FeaturesGL::~FeaturesGL() = default;

}  // namespace angle

#endif  // ANGLE_PLATFORM_FEATURESGL_H_
