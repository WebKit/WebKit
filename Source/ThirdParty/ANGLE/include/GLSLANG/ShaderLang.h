//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef GLSLANG_SHADERLANG_H_
#define GLSLANG_SHADERLANG_H_

#include <stddef.h>

#include "KHR/khrplatform.h"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

// Note: make sure to increment ANGLE_SH_VERSION when changing ShaderVars.h
#include "ShaderVars.h"

// Version number for shader translation API.
// It is incremented every time the API changes.
#define ANGLE_SH_VERSION 305

enum ShShaderSpec
{
    SH_GLES2_SPEC,
    SH_WEBGL_SPEC,

    SH_GLES3_SPEC,
    SH_WEBGL2_SPEC,

    SH_GLES3_1_SPEC,
    SH_WEBGL3_SPEC,

    SH_GLES3_2_SPEC,

    SH_GL_CORE_SPEC,
    SH_GL_COMPATIBILITY_SPEC,
};

enum ShShaderOutput
{
    // ESSL output only supported in some configurations.
    SH_ESSL_OUTPUT = 0x8B45,

    // GLSL output only supported in some configurations.
    SH_GLSL_COMPATIBILITY_OUTPUT = 0x8B46,
    // Note: GL introduced core profiles in 1.5.
    SH_GLSL_130_OUTPUT      = 0x8B47,
    SH_GLSL_140_OUTPUT      = 0x8B80,
    SH_GLSL_150_CORE_OUTPUT = 0x8B81,
    SH_GLSL_330_CORE_OUTPUT = 0x8B82,
    SH_GLSL_400_CORE_OUTPUT = 0x8B83,
    SH_GLSL_410_CORE_OUTPUT = 0x8B84,
    SH_GLSL_420_CORE_OUTPUT = 0x8B85,
    SH_GLSL_430_CORE_OUTPUT = 0x8B86,
    SH_GLSL_440_CORE_OUTPUT = 0x8B87,
    SH_GLSL_450_CORE_OUTPUT = 0x8B88,

    // Prefer using these to specify HLSL output type:
    SH_HLSL_3_0_OUTPUT       = 0x8B48,  // D3D 9
    SH_HLSL_4_1_OUTPUT       = 0x8B49,  // D3D 11
    SH_HLSL_4_0_FL9_3_OUTPUT = 0x8B4A,  // D3D 11 feature level 9_3

    // Output SPIR-V for the Vulkan backend.
    SH_SPIRV_VULKAN_OUTPUT = 0x8B4B,

    // Output SPIR-V to be cross compiled to Metal.
    SH_SPIRV_METAL_OUTPUT = 0x8B4C,

    // Output for MSL
    SH_MSL_METAL_OUTPUT = 0x8B4D,
};

// For ANGLE_shader_pixel_local_storage_coherent.
// Instructs the compiler which fragment synchronization method to use, if any.
enum class ShFragmentSynchronizationType
{
    NoSynchronization,

    FragmentShaderInterlock_NV_GL,
    FragmentShaderOrdering_INTEL_GL,
    FragmentShaderInterlock_ARB_GL,

    RasterizerOrderViews_D3D,

    InvalidEnum,
    EnumCount = InvalidEnum,
};

// Compile options.
struct ShCompileOptionsMetal
{
    // Direct-to-metal backend constants:

    // Binding index for driver uniforms:
    int driverUniformsBindingIndex;
    // Binding index for default uniforms:
    int defaultUniformsBindingIndex;
    // Binding index for UBO's argument buffer
    int UBOArgumentBufferBindingIndex;
};

struct ShCompileOptionsPLS
{
    // For ANGLE_shader_pixel_local_storage_coherent.
    ShFragmentSynchronizationType fragmentSynchronizationType;
};

struct ShCompileOptions
{
    ShCompileOptions();
    ShCompileOptions(const ShCompileOptions &other);
    ShCompileOptions &operator=(const ShCompileOptions &other);

    // Translates intermediate tree to glsl, hlsl, msl, or SPIR-V binary.  Can be queried by
    // calling sh::GetObjectCode().
    uint64_t objectCode : 1;

    // Extracts attributes, uniforms, and varyings.  Can be queried by calling ShGetVariableInfo().
    uint64_t variables : 1;

    // Tracks the source path for shaders.  Can be queried with getSourcePath().
    uint64_t sourcePath : 1;

    // Whether the internal representation of the AST should be output.
    uint64_t intermediateTree : 1;

    // If requested, validates the AST after every transformation.  Useful for debugging.
    uint64_t validateAST : 1;

    // Validates loop and indexing in the shader to ensure that they do not exceed the minimum
    // functionality mandated in GLSL 1.0 spec, Appendix A, Section 4 and 5.  There is no need to
    // specify this parameter when compiling for WebGL - it is implied.
    uint64_t validateLoopIndexing : 1;

    // Emits #line directives in HLSL.
    uint64_t lineDirectives : 1;

    // Due to spec difference between GLSL 4.1 or lower and ESSL3, some platforms (for example, Mac
    // OSX core profile) require a variable's "invariant"/"centroid" qualifiers to match between
    // vertex and fragment shader. A simple solution to allow such shaders to link is to omit the
    // two qualifiers.  AMD driver in Linux requires invariant qualifier to match between vertex and
    // fragment shaders, while ESSL3 disallows invariant qualifier in fragment shader and GLSL >=
    // 4.2 doesn't require invariant qualifier to match between shaders. Remove invariant qualifier
    // from vertex shader to workaround AMD driver bug.
    // Note that the two flags take effect on ESSL3 input shaders translated to GLSL 4.1 or lower
    // and to GLSL 4.2 or newer on Linux AMD.
    // TODO(zmo): This is not a good long-term solution. Simply dropping these qualifiers may break
    // some developers' content. A more complex workaround of dynamically generating, compiling, and
    // re-linking shaders that use these qualifiers should be implemented.
    uint64_t removeInvariantAndCentroidForESSL3 : 1;

    // This flag works around bug in Intel Mac drivers related to abs(i) where i is an integer.
    uint64_t emulateAbsIntFunction : 1;

    // Enforce the GLSL 1.017 Appendix A section 7 packing restrictions.  This flag only enforces
    // (and can only enforce) the packing restrictions for uniform variables in both vertex and
    // fragment shaders. ShCheckVariablesWithinPackingLimits() lets embedders enforce the packing
    // restrictions for varying variables during program link time.
    uint64_t enforcePackingRestrictions : 1;

    // This flag ensures all indirect (expression-based) array indexing is clamped to the bounds of
    // the array. This ensures, for example, that you cannot read off the end of a uniform, whether
    // an array vec234, or mat234 type.
    uint64_t clampIndirectArrayBounds : 1;

    // This flag limits the complexity of an expression.
    uint64_t limitExpressionComplexity : 1;

    // This flag limits the depth of the call stack.
    uint64_t limitCallStackDepth : 1;

    // This flag initializes gl_Position to vec4(0,0,0,0) at the beginning of the vertex shader's
    // main(), and has no effect in the fragment shader. It is intended as a workaround for drivers
    // which incorrectly fail to link programs if gl_Position is not written.
    uint64_t initGLPosition : 1;

    // This flag replaces
    //   "a && b" with "a ? b : false",
    //   "a || b" with "a ? true : b".
    // This is to work around a MacOSX driver bug that |b| is executed independent of |a|'s value.
    uint64_t unfoldShortCircuit : 1;

    // This flag initializes output variables to 0 at the beginning of main().  It is to avoid
    // undefined behaviors.
    uint64_t initOutputVariables : 1;

    // This flag scalarizes vec/ivec/bvec/mat constructor args.  It is intended as a workaround for
    // Linux/Mac driver bugs.
    uint64_t scalarizeVecAndMatConstructorArgs : 1;

    // This flag overwrites a struct name with a unique prefix.  It is intended as a workaround for
    // drivers that do not handle struct scopes correctly, including all Mac drivers and Linux AMD.
    uint64_t regenerateStructNames : 1;

    // This flag works around bugs in Mac drivers related to do-while by transforming them into an
    // other construct.
    uint64_t rewriteDoWhileLoops : 1;

    // This flag works around a bug in the HLSL compiler optimizer that folds certain constant pow
    // expressions incorrectly. Only applies to the HLSL back-end. It works by expanding the integer
    // pow expressions into a series of multiplies.
    uint64_t expandSelectHLSLIntegerPowExpressions : 1;

    // Flatten "#pragma STDGL invariant(all)" into the declarations of varying variables and
    // built-in GLSL variables. This compiler option is enabled automatically when needed.
    uint64_t flattenPragmaSTDGLInvariantAll : 1;

    // Some drivers do not take into account the base level of the texture in the results of the
    // HLSL GetDimensions builtin.  This flag instructs the compiler to manually add the base level
    // offsetting.
    uint64_t HLSLGetDimensionsIgnoresBaseLevel : 1;

    // This flag works around an issue in translating GLSL function texelFetchOffset on INTEL
    // drivers. It works by translating texelFetchOffset into texelFetch.
    uint64_t rewriteTexelFetchOffsetToTexelFetch : 1;

    // This flag works around condition bug of for and while loops in Intel Mac OSX drivers.
    // Condition calculation is not correct. Rewrite it from "CONDITION" to "CONDITION && true".
    uint64_t addAndTrueToLoopCondition : 1;

    // This flag works around a bug in evaluating unary minus operator on integer on some INTEL
    // drivers. It works by translating -(int) into ~(int) + 1.
    uint64_t rewriteIntegerUnaryMinusOperator : 1;

    // This flag works around a bug in evaluating isnan() on some INTEL D3D and Mac OSX drivers.  It
    // works by using an expression to emulate this function.
    uint64_t emulateIsnanFloatFunction : 1;

    // This flag will use all uniforms of unused std140 and shared uniform blocks at the beginning
    // of the vertex/fragment shader's main(). It is intended as a workaround for Mac drivers with
    // shader version 4.10. In those drivers, they will treat unused std140 and shared uniform
    // blocks' members as inactive. However, WebGL2.0 based on OpenGL ES3.0.4 requires all members
    // of a named uniform block declared with a shared or std140 layout qualifier to be considered
    // active. The uniform block itself is also considered active.
    uint64_t useUnusedStandardSharedBlocks : 1;

    // This flag works around a bug in unary minus operator on float numbers on Intel Mac OSX 10.11
    // drivers. It works by translating -float into 0.0 - float.
    uint64_t rewriteFloatUnaryMinusOperator : 1;

    // This flag works around a bug in evaluating atan(y, x) on some NVIDIA OpenGL drivers.  It
    // works by using an expression to emulate this function.
    uint64_t emulateAtan2FloatFunction : 1;

    // Set to initialize uninitialized local and global temporary variables. Should only be used
    // with GLSL output. In HLSL output variables are initialized regardless of if this flag is set.
    uint64_t initializeUninitializedLocals : 1;

    // The flag modifies the shader in the following way:
    //
    // Every occurrence of gl_InstanceID is replaced by the global temporary variable InstanceID.
    // Every occurrence of gl_ViewID_OVR is replaced by the varying variable ViewID_OVR.
    // At the beginning of the body of main() in a vertex shader the following initializers are
    // added:
    //   ViewID_OVR = uint(gl_InstanceID) % num_views;
    //   InstanceID = gl_InstanceID / num_views;
    // ViewID_OVR is added as a varying variable to both the vertex and fragment shaders.
    uint64_t initializeBuiltinsForInstancedMultiview : 1;

    // With the flag enabled the GLSL/ESSL vertex shader is modified to include code for viewport
    // selection in the following way:
    // - Code to enable the extension ARB_shader_viewport_layer_array/NV_viewport_array2 is
    // included.
    // - Code to select the viewport index or layer is inserted at the beginning of main after
    //   ViewID_OVR's initialization.
    // - A declaration of the uniform multiviewBaseViewLayerIndex.
    // Note: The initializeBuiltinsForInstancedMultiview flag also has to be enabled to have the
    // temporary variable ViewID_OVR declared and initialized.
    uint64_t selectViewInNvGLSLVertexShader : 1;

    // If the flag is enabled, gl_PointSize is clamped to the maximum point size specified in
    // ShBuiltInResources in vertex shaders.
    uint64_t clampPointSize : 1;

    // This flag indicates whether advanced blend equation should be emulated.  Currently only
    // implemented for the Vulkan backend.
    uint64_t addAdvancedBlendEquationsEmulation : 1;

    // Don't use loops to initialize uninitialized variables. Only has an effect if some kind of
    // variable initialization is turned on.
    uint64_t dontUseLoopsToInitializeVariables : 1;

    // Don't use D3D constant register zero when allocating space for uniforms. This is targeted to
    // work around a bug in NVIDIA D3D driver version 388.59 where in very specific cases the driver
    // would not handle constant register zero correctly. Only has an effect on HLSL translation.
    uint64_t skipD3DConstantRegisterZero : 1;

    // Clamp gl_FragDepth to the range [0.0, 1.0] in case it is statically used.
    uint64_t clampFragDepth : 1;

    // Rewrite expressions like "v.x = z = expression;". Works around a bug in NVIDIA OpenGL drivers
    // prior to version 397.31.
    uint64_t rewriteRepeatedAssignToSwizzled : 1;

    // Rewrite gl_DrawID as a uniform int
    uint64_t emulateGLDrawID : 1;

    // This flag initializes shared variables to 0.  It is to avoid ompute shaders being able to
    // read undefined values that could be coming from another webpage/application.
    uint64_t initSharedVariables : 1;

    // Forces the value returned from an atomic operations to be always be resolved. This is
    // targeted to workaround a bug in NVIDIA D3D driver where the return value from
    // RWByteAddressBuffer.InterlockedAdd does not get resolved when used in the .yzw components of
    // a RWByteAddressBuffer.Store operation. Only has an effect on HLSL translation.
    // http://anglebug.com/3246
    uint64_t forceAtomicValueResolution : 1;

    // Rewrite gl_BaseVertex and gl_BaseInstance as uniform int
    uint64_t emulateGLBaseVertexBaseInstance : 1;

    // Emulate seamful cube map sampling for OpenGL ES2.0.  Currently only applies to the Vulkan
    // backend, as is done after samplers are moved out of structs.  Can likely be made to work on
    // the other backends as well.
    uint64_t emulateSeamfulCubeMapSampling : 1;

    // This flag controls how to translate WEBGL_video_texture sampling function.
    uint64_t takeVideoTextureAsExternalOES : 1;

    // This flag works around a inconsistent behavior in Mac AMD driver where gl_VertexID doesn't
    // include base vertex value. It replaces gl_VertexID with (gl_VertexID + angle_BaseVertex) when
    // angle_BaseVertex is available.
    uint64_t addBaseVertexToVertexID : 1;

    // This works around the dynamic lvalue indexing of swizzled vectors on various platforms.
    uint64_t removeDynamicIndexingOfSwizzledVector : 1;

    // This flag works around a slow fxc compile performance issue with dynamic uniform indexing.
    uint64_t allowTranslateUniformBlockToStructuredBuffer : 1;

    // This flag allows us to add a decoration for layout(yuv) in shaders.
    uint64_t addVulkanYUVLayoutQualifier : 1;

    // This flag allows disabling ARB_texture_rectangle on a per-compile basis. This is necessary
    // for WebGL contexts becuase ARB_texture_rectangle may be necessary for the WebGL
    // implementation internally but shouldn't be exposed to WebGL user code.
    uint64_t disableARBTextureRectangle : 1;

    // This flag works around a driver bug by rewriting uses of row-major matrices as column-major
    // in ESSL 3.00 and greater shaders.
    uint64_t rewriteRowMajorMatrices : 1;

    // Drop any explicit precision qualifiers from shader.
    uint64_t ignorePrecisionQualifiers : 1;

    // Ask compiler to generate code for depth correction to conform to the Vulkan clip space.  If
    // VK_EXT_depth_clip_control is supported, this code is not generated, saving a uniform look up.
    uint64_t addVulkanDepthCorrection : 1;

    uint64_t forceShaderPrecisionHighpToMediump : 1;

    // Allow compiler to use specialization constant to do pre-rotation and y flip.
    uint64_t useSpecializationConstant : 1;

    // Ask compiler to generate Vulkan transform feedback emulation support code.
    uint64_t addVulkanXfbEmulationSupportCode : 1;

    // Ask compiler to generate Vulkan transform feedback support code when using the
    // VK_EXT_transform_feedback extension.
    uint64_t addVulkanXfbExtensionSupportCode : 1;

    // This flag initializes fragment shader's output variables to zero at the beginning of the
    // fragment shader's main(). It is intended as a workaround for drivers which get context lost
    // if gl_FragColor is not written.
    uint64_t initFragmentOutputVariables : 1;

    // Transitory flag to select between producing SPIR-V directly vs using glslang.  Ignored in
    // non-assert-enabled builds to avoid increasing ANGLE's binary size.
    uint64_t generateSpirvThroughGlslang : 1;

    // Insert explicit casts for float/double/unsigned/signed int on macOS 10.15 with Intel driver
    uint64_t addExplicitBoolCasts : 1;

    // Add round() after applying dither.  This works around a Qualcomm quirk where values can get
    // ceil()ed instead.
    uint64_t roundOutputAfterDithering : 1;

    // Even when the dividend and divisor have the same value some platforms do not return 1.0f.
    // Need to emit different division code for such platforms.
    uint64_t precisionSafeDivision : 1;

    // anglebug.com/7527: packUnorm4x8 fails on Pixel 4 if it is not passed a highp vec4.
    // TODO(anglebug.com/7527): This workaround is currently only applied for pixel local storage.
    // We may want to apply it generally.
    uint64_t passHighpToPackUnormSnormBuiltins : 1;

    ShCompileOptionsMetal metal;
    ShCompileOptionsPLS pls;
};

// The 64 bits hash function. The first parameter is the input string; the
// second parameter is the string length.
using ShHashFunction64 = khronos_uint64_t (*)(const char *, size_t);

//
// Implementation dependent built-in resources (constants and extensions).
// The names for these resources has been obtained by stripping gl_/GL_.
//
struct ShBuiltInResources
{
    ShBuiltInResources();
    ShBuiltInResources(const ShBuiltInResources &other);
    ShBuiltInResources &operator=(const ShBuiltInResources &other);

    // Constants.
    int MaxVertexAttribs;
    int MaxVertexUniformVectors;
    int MaxVaryingVectors;
    int MaxVertexTextureImageUnits;
    int MaxCombinedTextureImageUnits;
    int MaxTextureImageUnits;
    int MaxFragmentUniformVectors;
    int MaxDrawBuffers;

    // Extensions.
    // Set to 1 to enable the extension, else 0.
    int OES_standard_derivatives;
    int OES_EGL_image_external;
    int OES_EGL_image_external_essl3;
    int NV_EGL_stream_consumer_external;
    int ARB_texture_rectangle;
    int EXT_blend_func_extended;
    int EXT_draw_buffers;
    int EXT_frag_depth;
    int EXT_shader_texture_lod;
    int EXT_shader_framebuffer_fetch;
    int EXT_shader_framebuffer_fetch_non_coherent;
    int NV_shader_framebuffer_fetch;
    int NV_shader_noperspective_interpolation;
    int ARM_shader_framebuffer_fetch;
    int OVR_multiview;
    int OVR_multiview2;
    int EXT_multisampled_render_to_texture;
    int EXT_multisampled_render_to_texture2;
    int EXT_YUV_target;
    int EXT_geometry_shader;
    int OES_geometry_shader;
    int OES_shader_io_blocks;
    int EXT_shader_io_blocks;
    int EXT_gpu_shader5;
    int EXT_shader_non_constant_global_initializers;
    int OES_texture_storage_multisample_2d_array;
    int OES_texture_3D;
    int ANGLE_shader_pixel_local_storage;
    int ANGLE_texture_multisample;
    int ANGLE_multi_draw;
    // TODO(angleproject:3402) remove after chromium side removal to pass compilation
    int ANGLE_base_vertex_base_instance;
    int WEBGL_video_texture;
    int APPLE_clip_distance;
    int OES_texture_cube_map_array;
    int EXT_texture_cube_map_array;
    int EXT_shadow_samplers;
    int OES_shader_multisample_interpolation;
    int OES_shader_image_atomic;
    int EXT_tessellation_shader;
    int OES_texture_buffer;
    int EXT_texture_buffer;
    int OES_sample_variables;
    int EXT_clip_cull_distance;
    int EXT_primitive_bounding_box;
    int OES_primitive_bounding_box;
    int ANGLE_base_vertex_base_instance_shader_builtin;
    int ANDROID_extension_pack_es31a;
    int KHR_blend_equation_advanced;

    // Set to 1 to enable replacing GL_EXT_draw_buffers #extension directives
    // with GL_NV_draw_buffers in ESSL output. This flag can be used to emulate
    // EXT_draw_buffers by using it in combination with GLES3.0 glDrawBuffers
    // function. This applies to Tegra K1 devices.
    int NV_draw_buffers;

    // Set to 1 if highp precision is supported in the ESSL 1.00 version of the
    // fragment language. Does not affect versions of the language where highp
    // support is mandatory.
    // Default is 0.
    int FragmentPrecisionHigh;

    // GLSL ES 3.0 constants.
    int MaxVertexOutputVectors;
    int MaxFragmentInputVectors;
    int MinProgramTexelOffset;
    int MaxProgramTexelOffset;

    // Extension constants.

    // Value of GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT for OpenGL ES output context.
    // Value of GL_MAX_DUAL_SOURCE_DRAW_BUFFERS for OpenGL output context.
    // GLES SL version 100 gl_MaxDualSourceDrawBuffersEXT value for EXT_blend_func_extended.
    int MaxDualSourceDrawBuffers;

    // Value of GL_MAX_VIEWS_OVR.
    int MaxViewsOVR;

    // Name Hashing.
    // Set a 64 bit hash function to enable user-defined name hashing.
    // Default is NULL.
    ShHashFunction64 HashFunction;

    // The maximum complexity an expression can be when limitExpressionComplexity is turned on.
    int MaxExpressionComplexity;

    // The maximum depth a call stack can be.
    int MaxCallStackDepth;

    // The maximum number of parameters a function can have when limitExpressionComplexity is turned
    // on.
    int MaxFunctionParameters;

    // GLES 3.1 constants

    // texture gather offset constraints.
    int MinProgramTextureGatherOffset;
    int MaxProgramTextureGatherOffset;

    // maximum number of available image units
    int MaxImageUnits;

    // OES_sample_variables constant
    // maximum number of available samples
    int MaxSamples;

    // maximum number of image uniforms in a vertex shader
    int MaxVertexImageUniforms;

    // maximum number of image uniforms in a fragment shader
    int MaxFragmentImageUniforms;

    // maximum number of image uniforms in a compute shader
    int MaxComputeImageUniforms;

    // maximum total number of image uniforms in a program
    int MaxCombinedImageUniforms;

    // maximum number of uniform locations
    int MaxUniformLocations;

    // maximum number of ssbos and images in a shader
    int MaxCombinedShaderOutputResources;

    // maximum number of groups in each dimension
    std::array<int, 3> MaxComputeWorkGroupCount;
    // maximum number of threads per work group in each dimension
    std::array<int, 3> MaxComputeWorkGroupSize;

    // maximum number of total uniform components
    int MaxComputeUniformComponents;

    // maximum number of texture image units in a compute shader
    int MaxComputeTextureImageUnits;

    // maximum number of atomic counters in a compute shader
    int MaxComputeAtomicCounters;

    // maximum number of atomic counter buffers in a compute shader
    int MaxComputeAtomicCounterBuffers;

    // maximum number of atomic counters in a vertex shader
    int MaxVertexAtomicCounters;

    // maximum number of atomic counters in a fragment shader
    int MaxFragmentAtomicCounters;

    // maximum number of atomic counters in a program
    int MaxCombinedAtomicCounters;

    // maximum binding for an atomic counter
    int MaxAtomicCounterBindings;

    // maximum number of atomic counter buffers in a vertex shader
    int MaxVertexAtomicCounterBuffers;

    // maximum number of atomic counter buffers in a fragment shader
    int MaxFragmentAtomicCounterBuffers;

    // maximum number of atomic counter buffers in a program
    int MaxCombinedAtomicCounterBuffers;

    // maximum number of buffer object storage in machine units
    int MaxAtomicCounterBufferSize;

    // maximum number of uniform block bindings
    int MaxUniformBufferBindings;

    // maximum number of shader storage buffer bindings
    int MaxShaderStorageBufferBindings;

    // maximum point size (higher limit from ALIASED_POINT_SIZE_RANGE)
    float MaxPointSize;

    // EXT_geometry_shader constants
    int MaxGeometryUniformComponents;
    int MaxGeometryUniformBlocks;
    int MaxGeometryInputComponents;
    int MaxGeometryOutputComponents;
    int MaxGeometryOutputVertices;
    int MaxGeometryTotalOutputComponents;
    int MaxGeometryTextureImageUnits;
    int MaxGeometryAtomicCounterBuffers;
    int MaxGeometryAtomicCounters;
    int MaxGeometryShaderStorageBlocks;
    int MaxGeometryShaderInvocations;
    int MaxGeometryImageUniforms;

    // EXT_tessellation_shader constants
    int MaxTessControlInputComponents;
    int MaxTessControlOutputComponents;
    int MaxTessControlTextureImageUnits;
    int MaxTessControlUniformComponents;
    int MaxTessControlTotalOutputComponents;
    int MaxTessControlImageUniforms;
    int MaxTessControlAtomicCounters;
    int MaxTessControlAtomicCounterBuffers;

    int MaxTessPatchComponents;
    int MaxPatchVertices;
    int MaxTessGenLevel;

    int MaxTessEvaluationInputComponents;
    int MaxTessEvaluationOutputComponents;
    int MaxTessEvaluationTextureImageUnits;
    int MaxTessEvaluationUniformComponents;
    int MaxTessEvaluationImageUniforms;
    int MaxTessEvaluationAtomicCounters;
    int MaxTessEvaluationAtomicCounterBuffers;

    // Subpixel bits used in rasterization.
    int SubPixelBits;

    // APPLE_clip_distance/EXT_clip_cull_distance constant
    int MaxClipDistances;
    int MaxCullDistances;
    int MaxCombinedClipAndCullDistances;
};

//
// ShHandle held by but opaque to the driver.  It is allocated,
// managed, and de-allocated by the compiler. Its contents
// are defined by and used by the compiler.
//
// If handle creation fails, 0 will be returned.
//
using ShHandle = void *;

namespace sh
{
using BinaryBlob = std::vector<uint32_t>;

//
// Driver must call this first, once, before doing any other compiler operations.
// If the function succeeds, the return value is true, else false.
//
bool Initialize();
//
// Driver should call this at shutdown.
// If the function succeeds, the return value is true, else false.
//
bool Finalize();

//
// Initialize built-in resources with minimum expected values.
// Parameters:
// resources: The object to initialize. Will be comparable with memcmp.
//
void InitBuiltInResources(ShBuiltInResources *resources);

//
// Returns a copy of the current ShBuiltInResources stored in the compiler.
// Parameters:
// handle: Specifies the handle of the compiler to be used.
ShBuiltInResources GetBuiltInResources(const ShHandle handle);

//
// Returns the a concatenated list of the items in ShBuiltInResources as a null-terminated string.
// This function must be updated whenever ShBuiltInResources is changed.
// Parameters:
// handle: Specifies the handle of the compiler to be used.
const std::string &GetBuiltInResourcesString(const ShHandle handle);

//
// Driver calls these to create and destroy compiler objects.
//
// Returns the handle of constructed compiler, null if the requested compiler is not supported.
// Parameters:
// type: Specifies the type of shader - GL_FRAGMENT_SHADER or GL_VERTEX_SHADER.
// spec: Specifies the language spec the compiler must conform to - SH_GLES2_SPEC or SH_WEBGL_SPEC.
// output: Specifies the output code type - for example SH_ESSL_OUTPUT, SH_GLSL_OUTPUT,
//         SH_HLSL_3_0_OUTPUT or SH_HLSL_4_1_OUTPUT. Note: Each output type may only
//         be supported in some configurations.
// resources: Specifies the built-in resources.
ShHandle ConstructCompiler(sh::GLenum type,
                           ShShaderSpec spec,
                           ShShaderOutput output,
                           const ShBuiltInResources *resources);
void Destruct(ShHandle handle);

//
// Compiles the given shader source.
// If the function succeeds, the return value is true, else false.
// Parameters:
// handle: Specifies the handle of compiler to be used.
// shaderStrings: Specifies an array of pointers to null-terminated strings containing the shader
// source code.
// numStrings: Specifies the number of elements in shaderStrings array.
// compileOptions: A mask of compile options defined above.
bool Compile(const ShHandle handle,
             const char *const shaderStrings[],
             size_t numStrings,
             const ShCompileOptions &compileOptions);

// Clears the results from the previous compilation.
void ClearResults(const ShHandle handle);

// Return the version of the shader language.
int GetShaderVersion(const ShHandle handle);

// Return the currently set language output type.
ShShaderOutput GetShaderOutputType(const ShHandle handle);

// Returns null-terminated information log for a compiled shader.
// Parameters:
// handle: Specifies the compiler
const std::string &GetInfoLog(const ShHandle handle);

// Returns null-terminated object code for a compiled shader.  Only valid for output types that
// generate human-readable code (GLSL, ESSL or HLSL).
// Parameters:
// handle: Specifies the compiler
const std::string &GetObjectCode(const ShHandle handle);

// Returns object binary blob for a compiled shader.  Only valid for output types that
// generate binary blob (SPIR-V).
// Parameters:
// handle: Specifies the compiler
const BinaryBlob &GetObjectBinaryBlob(const ShHandle handle);

// Returns a (original_name, hash) map containing all the user defined names in the shader,
// including variable names, function names, struct names, and struct field names.
// Parameters:
// handle: Specifies the compiler
const std::map<std::string, std::string> *GetNameHashingMap(const ShHandle handle);

// Shader variable inspection.
// Returns a pointer to a list of variables of the designated type.
// (See ShaderVars.h for type definitions, included above)
// Returns NULL on failure.
// Parameters:
// handle: Specifies the compiler
const std::vector<sh::ShaderVariable> *GetUniforms(const ShHandle handle);
const std::vector<sh::ShaderVariable> *GetVaryings(const ShHandle handle);
const std::vector<sh::ShaderVariable> *GetInputVaryings(const ShHandle handle);
const std::vector<sh::ShaderVariable> *GetOutputVaryings(const ShHandle handle);
const std::vector<sh::ShaderVariable> *GetAttributes(const ShHandle handle);
const std::vector<sh::ShaderVariable> *GetOutputVariables(const ShHandle handle);
const std::vector<sh::InterfaceBlock> *GetInterfaceBlocks(const ShHandle handle);
const std::vector<sh::InterfaceBlock> *GetUniformBlocks(const ShHandle handle);
const std::vector<sh::InterfaceBlock> *GetShaderStorageBlocks(const ShHandle handle);
sh::WorkGroupSize GetComputeShaderLocalGroupSize(const ShHandle handle);
// Returns the number of views specified through the num_views layout qualifier. If num_views is
// not set, the function returns -1.
int GetVertexShaderNumViews(const ShHandle handle);
// Returns true if the shader has specified the |sample| qualifier, implying that per-sample shading
// should be enabled
bool EnablesPerSampleShading(const ShHandle handle);

// Returns specialization constant usage bits
uint32_t GetShaderSpecConstUsageBits(const ShHandle handle);

// Returns true if the passed in variables pack in maxVectors followingthe packing rules from the
// GLSL 1.017 spec, Appendix A, section 7.
// Returns false otherwise. Also look at the enforcePackingRestrictions flag above.
// Parameters:
// maxVectors: the available rows of registers.
// variables: an array of variables.
bool CheckVariablesWithinPackingLimits(int maxVectors,
                                       const std::vector<sh::ShaderVariable> &variables);

// Gives the compiler-assigned register for a shader storage block.
// The method writes the value to the output variable "indexOut".
// Returns true if it found a valid shader storage block, false otherwise.
// Parameters:
// handle: Specifies the compiler
// shaderStorageBlockName: Specifies the shader storage block
// indexOut: output variable that stores the assigned register
bool GetShaderStorageBlockRegister(const ShHandle handle,
                                   const std::string &shaderStorageBlockName,
                                   unsigned int *indexOut);

// Gives the compiler-assigned register for a uniform block.
// The method writes the value to the output variable "indexOut".
// Returns true if it found a valid uniform block, false otherwise.
// Parameters:
// handle: Specifies the compiler
// uniformBlockName: Specifies the uniform block
// indexOut: output variable that stores the assigned register
bool GetUniformBlockRegister(const ShHandle handle,
                             const std::string &uniformBlockName,
                             unsigned int *indexOut);

bool ShouldUniformBlockUseStructuredBuffer(const ShHandle handle,
                                           const std::string &uniformBlockName);
const std::set<std::string> *GetSlowCompilingUniformBlockSet(const ShHandle handle);

// Gives a map from uniform names to compiler-assigned registers in the default uniform block.
// Note that the map contains also registers of samplers that have been extracted from structs.
const std::map<std::string, unsigned int> *GetUniformRegisterMap(const ShHandle handle);

// Sampler, image and atomic counters share registers(t type and u type),
// GetReadonlyImage2DRegisterIndex and GetImage2DRegisterIndex return the first index into
// a range of reserved registers for image2D/iimage2D/uimage2D variables.
// Parameters: handle: Specifies the compiler
unsigned int GetReadonlyImage2DRegisterIndex(const ShHandle handle);
unsigned int GetImage2DRegisterIndex(const ShHandle handle);

// The method records these used function names related with image2D/iimage2D/uimage2D, these
// functions will be dynamically generated.
// Parameters:
// handle: Specifies the compiler
const std::set<std::string> *GetUsedImage2DFunctionNames(const ShHandle handle);

bool HasDiscardInFragmentShader(const ShHandle handle);
bool HasValidGeometryShaderInputPrimitiveType(const ShHandle handle);
bool HasValidGeometryShaderOutputPrimitiveType(const ShHandle handle);
bool HasValidGeometryShaderMaxVertices(const ShHandle handle);
bool HasValidTessGenMode(const ShHandle handle);
bool HasValidTessGenSpacing(const ShHandle handle);
bool HasValidTessGenVertexOrder(const ShHandle handle);
bool HasValidTessGenPointMode(const ShHandle handle);
GLenum GetGeometryShaderInputPrimitiveType(const ShHandle handle);
GLenum GetGeometryShaderOutputPrimitiveType(const ShHandle handle);
int GetGeometryShaderInvocations(const ShHandle handle);
int GetGeometryShaderMaxVertices(const ShHandle handle);
unsigned int GetShaderSharedMemorySize(const ShHandle handle);
int GetTessControlShaderVertices(const ShHandle handle);
GLenum GetTessGenMode(const ShHandle handle);
GLenum GetTessGenSpacing(const ShHandle handle);
GLenum GetTessGenVertexOrder(const ShHandle handle);
GLenum GetTessGenPointMode(const ShHandle handle);

// Returns the blend equation list supported in the fragment shader.  This is a bitset of
// gl::BlendEquationType, and can only include bits from KHR_blend_equation_advanced.
uint32_t GetAdvancedBlendEquations(const ShHandle handle);

//
// Helper function to identify specs that are based on the WebGL spec.
//
inline bool IsWebGLBasedSpec(ShShaderSpec spec)
{
    return (spec == SH_WEBGL_SPEC || spec == SH_WEBGL2_SPEC || spec == SH_WEBGL3_SPEC);
}

//
// Helper function to identify DesktopGL specs
//
inline bool IsDesktopGLSpec(ShShaderSpec spec)
{
    return spec == SH_GL_CORE_SPEC || spec == SH_GL_COMPATIBILITY_SPEC;
}

// Can't prefix with just _ because then we might introduce a double underscore, which is not safe
// in GLSL (ESSL 3.00.6 section 3.8: All identifiers containing a double underscore are reserved for
// use by the underlying implementation). u is short for user-defined.
extern const char kUserDefinedNamePrefix[];

namespace vk
{

// Specialization constant ids
enum class SpecializationConstantId : uint32_t
{
    SurfaceRotation = 0,
    Dither          = 1,

    InvalidEnum = 2,
    EnumCount   = InvalidEnum,
};

enum class SpecConstUsage : uint32_t
{
    Rotation = 0,
    Dither   = 1,

    InvalidEnum = 2,
    EnumCount   = InvalidEnum,
};

enum ColorAttachmentDitherControl
{
    // See comments in ContextVk::updateDither and EmulateDithering.cpp
    kDitherControlNoDither   = 0,
    kDitherControlDither4444 = 1,
    kDitherControlDither5551 = 2,
    kDitherControlDither565  = 3,
};

// Interface block name containing the aggregate default uniforms
extern const char kDefaultUniformsNameVS[];
extern const char kDefaultUniformsNameTCS[];
extern const char kDefaultUniformsNameTES[];
extern const char kDefaultUniformsNameGS[];
extern const char kDefaultUniformsNameFS[];
extern const char kDefaultUniformsNameCS[];

// Interface block and variable names containing driver uniforms
extern const char kDriverUniformsBlockName[];
extern const char kDriverUniformsVarName[];

// Packing information for driver uniform's misc field:
// - 1 bit for whether surface rotation results in swapped axes
// - 5 bits for advanced blend equation
// - 6 bits for sample count
// - 8 bits for enabled clip planes
// - 1 bit for whether depth should be transformed to Vulkan clip space
// - 11 bits unused
constexpr uint32_t kDriverUniformsMiscSwapXYMask                  = 0x1;
constexpr uint32_t kDriverUniformsMiscAdvancedBlendEquationOffset = 1;
constexpr uint32_t kDriverUniformsMiscAdvancedBlendEquationMask   = 0x1F;
constexpr uint32_t kDriverUniformsMiscSampleCountOffset           = 6;
constexpr uint32_t kDriverUniformsMiscSampleCountMask             = 0x3F;
constexpr uint32_t kDriverUniformsMiscEnabledClipPlanesOffset     = 12;
constexpr uint32_t kDriverUniformsMiscEnabledClipPlanesMask       = 0xFF;
constexpr uint32_t kDriverUniformsMiscTransformDepthOffset        = 20;
constexpr uint32_t kDriverUniformsMiscTransformDepthMask          = 0x1;

// Interface block array name used for atomic counter emulation
extern const char kAtomicCountersBlockName[];

// Transform feedback emulation support
extern const char kXfbEmulationGetOffsetsFunctionName[];
extern const char kXfbEmulationCaptureFunctionName[];
extern const char kXfbEmulationBufferBlockName[];
extern const char kXfbEmulationBufferName[];
extern const char kXfbEmulationBufferFieldName[];

// Transform feedback extension support
extern const char kXfbExtensionPositionOutName[];

// Pre-rotation support
extern const char kTransformPositionFunctionName[];

// EXT_shader_framebuffer_fetch and EXT_shader_framebuffer_fetch_non_coherent
extern const char kInputAttachmentName[];

}  // namespace vk

namespace mtl
{
// Specialization constant to enable GL_SAMPLE_COVERAGE_VALUE emulation.
extern const char kCoverageMaskEnabledConstName[];

// Specialization constant to emulate rasterizer discard.
extern const char kRasterizerDiscardEnabledConstName[];

// Specialization constant to enable depth write in fragment shaders.
extern const char kDepthWriteEnabledConstName[];
}  // namespace mtl

// For backends that use glslang (the Vulkan shader compiler), i.e. Vulkan and Metal, call these to
// initialize and finalize glslang itself.  This can be called independently from Initialize() and
// Finalize().
void InitializeGlslang();
void FinalizeGlslang();

}  // namespace sh

#endif  // GLSLANG_SHADERLANG_H_
