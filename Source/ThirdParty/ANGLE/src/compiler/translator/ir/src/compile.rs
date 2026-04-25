// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The compile process, including IR transformations and output generation.

use crate::ir::*;
use crate::*;

#[cxx::bridge(namespace = "sh::ir::ffi")]
mod ffi {
    // TODO(http://anglebug.com/349994211): equivalent enums to the options in ShaderLang.h, eventually all options need to be
    // passed to IR: add them as the translator is converted to IR.

    // Matching ShShaderOutput
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum OutputLanguage {
        Null,
        Essl,
        Glsl150Core,
        Glsl330Core,
        Glsl400Core,
        Glsl410Core,
        Glsl420Core,
        Glsl430Core,
        Glsl440Core,
        Glsl450Core,
        Hlsl3,
        Hlsl41,
        Spirv,
        Msl,
        Wgsl,
    }

    // List of enabled extensions
    struct ExtensionsEnabled {
        ANDROID_extension_pack_es31a: bool,
        ANGLE_base_vertex_base_instance_shader_builtin: bool,
        ANGLE_clip_cull_distance: bool,
        ANGLE_multi_draw: bool,
        ANGLE_shader_pixel_local_storage: bool,
        ANGLE_texture_multisample: bool,
        APPLE_clip_distance: bool,
        ARB_fragment_shader_interlock: bool,
        ARB_texture_rectangle: bool,
        ARM_shader_framebuffer_fetch: bool,
        ARM_shader_framebuffer_fetch_depth_stencil: bool,
        EXT_YUV_target: bool,
        EXT_blend_func_extended: bool,
        EXT_clip_cull_distance: bool,
        EXT_conservative_depth: bool,
        EXT_draw_buffers: bool,
        EXT_frag_depth: bool,
        EXT_fragment_shading_rate: bool,
        EXT_fragment_shading_rate_primitive: bool,
        EXT_geometry_shader: bool,
        EXT_gpu_shader5: bool,
        EXT_primitive_bounding_box: bool,
        EXT_separate_shader_objects: bool,
        EXT_shader_framebuffer_fetch: bool,
        EXT_shader_framebuffer_fetch_non_coherent: bool,
        EXT_shader_io_blocks: bool,
        EXT_shader_non_constant_global_initializers: bool,
        EXT_shader_texture_lod: bool,
        EXT_shadow_samplers: bool,
        EXT_tessellation_shader: bool,
        EXT_texture_buffer: bool,
        EXT_texture_cube_map_array: bool,
        EXT_texture_query_lod: bool,
        EXT_texture_shadow_lod: bool,
        INTEL_fragment_shader_ordering: bool,
        KHR_blend_equation_advanced: bool,
        NV_EGL_stream_consumer_external: bool,
        NV_fragment_shader_interlock: bool,
        NV_shader_noperspective_interpolation: bool,
        OES_EGL_image_external: bool,
        OES_EGL_image_external_essl3: bool,
        OES_geometry_shader: bool,
        OES_gpu_shader5: bool,
        OES_primitive_bounding_box: bool,
        OES_sample_variables: bool,
        OES_shader_image_atomic: bool,
        OES_shader_io_blocks: bool,
        OES_shader_multisample_interpolation: bool,
        OES_standard_derivatives: bool,
        OES_tessellation_shader: bool,
        OES_texture_3D: bool,
        OES_texture_buffer: bool,
        OES_texture_cube_map_array: bool,
        OES_texture_storage_multisample_2d_array: bool,
        OVR_multiview: bool,
        OVR_multiview2: bool,
        WEBGL_video_texture: bool,
    }

    // Limits corresponding to ShBuiltInResources
    struct Limits {
        max_draw_buffers: u32,
        max_dual_source_draw_buffers: u32,
        max_combined_draw_buffers_and_pixel_local_storage_planes: u32,
        min_point_size: f32,
        max_point_size: f32,
        max_expression_complexity: u32,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum PixelLocalStorageImpl {
        NotSupported,
        ImageLoadStore,
        FramebufferFetch,
    }

    #[derive(Copy, Clone, PartialEq)]
    #[repr(u32)]
    enum PixelLocalStorageSync {
        // Fragments cannot be ordered or synchronized.
        NotSupported,

        // Fragments are automatically raster-ordered and synchronized.
        Automatic,

        // Various shader interlock GLSL vendor extensions.
        FragmentShaderInterlockNV_GL,
        FragmentShaderOrderingINTEL_GL,
        // The ARB enum is used to include usage of SPV_EXT_fragment_shader_interlock in SPIR-V
        // generator.
        FragmentShaderInterlockARB_GL,

        // D3D shader interlock.
        RasterizerOrderViews_D3D,

        // Metal shader interlock.
        RasterOrderGroups_Metal,
    }

    #[derive(Copy, Clone)]
    struct PixelLocalStorageOptions {
        implementation: PixelLocalStorageImpl,
        // For ANGLE_shader_pixel_local_storage_coherent.
        fragment_sync: PixelLocalStorageSync,
        // Apple Silicon doesn't support image memory barriers, and many GL devices don't support
        // noncoherent framebuffer fetch. On these platforms, we simply ignore the "noncoherent"
        // PLS qualifier.
        supports_noncoherent: bool,
        // PixelLocalStorageImpl::ImageLoadStore only: Can we use rgba8/rgba8i/rgba8ui image
        // formats, or do we need to manually pack and unpack from r32i/r32ui?
        supports_native_rgba8_image_formats: bool,
    }

    struct CompileOptions {
        // Input shader and device properties:

        // The version of the input version
        shader_version: i32,
        // Extensions that were enabled, mostly useful for the GLSL/ESSL output to replicate them.
        extensions: ExtensionsEnabled,
        // Limits set by the API.
        limits: Limits,

        // Flags controlling the output:

        // What is the output of the compiler (SPIR-V, GLSL, WGSL, etc).
        output: OutputLanguage,
        // Whether this is an ES1 shader.  This is used while the output is AST.  Currently, the
        // ESSL output always matches the input shader version.  Once the ESSL output is made
        // independent (for example outputting ESSL 300 even if the input is ESSL 100), then the
        // _output_ version should be used by the generator.
        is_es1: bool,

        // Whether uninitialized local and global variables should be zero-initialized.
        initialize_uninitialized_variables: bool,
        // Whether loops can be used when zero-initializing variables.
        loops_allowed_when_initializing_variables: bool,
        // Whether non-const variables in global scope can have an initializer
        initializer_allowed_on_non_constant_global_variables: bool,
        // Work around driver bug where pack/unpack built-ins cannot consume/produce mediump vec4
        // without also truncating the uint.
        // Note: This is currently not a generalized workaround, just applied to Pixel Local
        // Storage emulation, but in theory it should be.
        pass_highp_to_pack_unorm_snorm_built_ins: bool,
        // Whether gl_ViewID_OVR and gl_InstanceID variables need to be emulated for multiview.
        emulate_instanced_multiview: bool,
        // Select viewport layer/index if ARB_shader_viewport_layer_array/NV_viewport_array2 is
        // used to implement multiview.  Requires emulate_instanced_multiview.
        select_viewport_layer_in_emulated_multiview: bool,
        // Whether gl_DrawID need to be emulated.
        emulate_draw_id: bool,
        // Whether gl_BaseVertex and gl_BaseInstance need to be emulated.
        emulate_base_vertex_instance: bool,
        // Whether gl_BaseVertex should be added to gl_VertexID as a driver bug workaround.  Only
        // effective if `emulate_base_vertex_instance`.
        add_base_vertex_to_vertex_id: bool,
        // If the flag is enabled, gl_PointSize is clamped to the maximum point size specified in
        // ShBuiltInResources in vertex shaders.
        clamp_point_size: bool,
        // Clamp gl_FragDepth to the range [0.0, 1.0].
        clamp_frag_depth: bool,
        // Whether lowp and mediump float uniforms should be translated as fp16.
        transform_float_uniform_to_fp16: bool,
        // Whether inactive variables should be removed, and of those, whether inactive fragments
        // outputs in particular should be removed too.
        remove_inactive_interface_variables: bool,
        retain_inactive_fragment_outputs: bool,
        // Whether vec and mat constructor args should be broken down into scalars.
        scalarize_vec_and_mat_constructor_args: bool,
        // Clamp non-constant indices to the bounds of the entity being indexed for robustness.
        clamp_indirect_indices: bool,

        // Whether the ANGLE_pixel_local_storage extension has been used and there are PLS uniforms
        // to rewrite.
        rewrite_pixel_local_storage: bool,
        pls_options: PixelLocalStorageOptions,

        // MSL: Ensure all loops execute side-effects or terminate.
        ensure_loop_forward_progress: bool,
    }

    // Matching sh::InterpolationType
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum Interpolation {
        Smooth,
        Flat,
        NoPerspective,
        Centroid,
        Sample,
        NoPerspectiveCentroid,
        NoPerspectiveSample,
    }

    // Matching sh::BlockLayoutType
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum BlockLayout {
        Std140,
        Std430,
        Packed,
        Shared,
    }

    // Matching sh::BlockType
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum BlockType {
        Uniform,
        Buffer,
    }

    // Corresponding to sh::ShaderVariable
    struct ShaderVariable {
        gl_type: u32,
        gl_precision: u32,
        name: String,
        mapped_name: String,

        // Used by varyings of struct type, or I/O blocks.
        struct_or_block_name: String,
        mapped_struct_or_block_name: String,
        fields: Vec<ShaderVariable>,

        // Outermost array size is stored at the end of the vector.
        array_sizes: Vec<u32>,

        static_use: bool,
        active: bool,

        // Only applies to I/O block fields:
        is_row_major: bool,

        // Varyings and fragment outputs:
        location: i32,
        // Distinguish between shader-specified location and implicitly-derived location
        is_location_implicit: bool,

        // Varyings:
        interpolation: Interpolation,
        is_invariant: bool,
        is_io_block: bool,
        is_patch: bool,

        // Uniforms:
        binding: i32,
        gl_image_unit_format: u32,
        offset: i32,
        raster_ordered: bool,
        readonly: bool,
        writeonly: bool,
        // If the variable is a sampler that has ever been statically used with texelFetch:
        texel_fetch_static_use: bool,
        // If the default uniform is given an fp16 type:
        is_float16: bool,

        // EXT_shader_framebuffer_fetch / KHR_blend_equation_advanced:
        is_fragment_inout: bool,

        // EXT_blend_func_extended:
        index: i32,

        // EXT_YUV_target:
        yuv: bool,

        // SPIR-V ID:
        id: u32,
    }

    // Corresponding to sh::InterfaceBlock
    struct InterfaceBlock {
        name: String,
        mapped_name: String,
        instance_name: String,

        fields: Vec<ShaderVariable>,

        static_use: bool,
        active: bool,

        array_size: u32,
        block_type: BlockType,
        block_layout: BlockLayout,
        binding: i32,

        // SSBOs:
        readonly: bool,

        // SPIR-V ID:
        id: u32,
    }

    struct Output {
        // Note: For now, generate always results in an AST. Eventually either text or binary
        // should be returned.
        ast: *mut TIntermBlock,

        // Reflection info
        inputs: Vec<ShaderVariable>,
        outputs: Vec<ShaderVariable>,
        uniforms: Vec<ShaderVariable>,
        shared: Vec<ShaderVariable>,
        uniform_blocks: Vec<InterfaceBlock>,
        storage_blocks: Vec<InterfaceBlock>,
    }

    extern "C++" {
        include!("compiler/translator/ir/src/output/legacy.h");

        #[namespace = "sh"]
        type TCompiler = crate::output::legacy::ffi::TCompiler;

        #[namespace = "sh"]
        type TIntermBlock = crate::output::legacy::ffi::TIntermBlock;

        // Work around an issue similar to http://crbug.com/418073233 where the translator is
        // statically linked to both libGLESv2.so and angle_end2end_tests, and crossing the
        // C++->Rust boundary somehow jumps between the two copies, messing up global variables.
        #[namespace = "angle"]
        type PoolAllocator;

        include!("compiler/translator/ir/src/builder.rs.h");
        type IR = crate::ir::IR;

        include!("compiler/translator/ir/src/pool_alloc.h");
        unsafe fn initialize_global_pool_index();
        unsafe fn free_global_pool_index();
        unsafe fn set_global_pool_allocator(allocator: *mut PoolAllocator);
    }
    extern "Rust" {
        unsafe fn generate_ast(
            mut ir: Box<IR>,
            compiler: *mut TCompiler,
            allocator: *mut PoolAllocator,
            options: &CompileOptions,
        ) -> Output;

        fn initialize_global_pool_index_workaround();
        fn free_global_pool_index_workaround();
    }
}

pub use ffi::BlockLayout;
pub use ffi::BlockType;
pub use ffi::CompileOptions as Options;
pub use ffi::InterfaceBlock;
pub use ffi::Interpolation;
pub use ffi::OutputLanguage;
pub use ffi::PixelLocalStorageImpl;
pub use ffi::PixelLocalStorageOptions;
pub use ffi::PixelLocalStorageSync;
pub use ffi::ShaderVariable;

unsafe fn generate_ast(
    mut ir: Box<IR>,
    compiler: *mut ffi::TCompiler,
    allocator: *mut ffi::PoolAllocator,
    options: &Options,
) -> ffi::Output {
    unsafe { ffi::set_global_pool_allocator(allocator) };

    // Apply transforms shared by multiple generators:
    common_pre_variable_collection_transforms(&mut ir, options);
    collect_reflection_info(&mut ir, options);
    common_post_variable_collection_transforms(&mut ir, options);

    // Generator-specific transformations and codegen.  Note that currently no codegen is actually
    // implemented from IR, so these would only do transformations and the common IR->AST generator
    // is used for all.
    match options.output {
        OutputLanguage::Null => output::null::generate(&mut ir, options),
        OutputLanguage::Essl => {
            #[cfg(angle_enable_essl)]
            output::essl::generate(&mut ir, options);
            #[cfg(not(angle_enable_essl))]
            panic!("Internal error: ESSL generator is not built");
        }
        OutputLanguage::Glsl150Core
        | OutputLanguage::Glsl330Core
        | OutputLanguage::Glsl400Core
        | OutputLanguage::Glsl410Core
        | OutputLanguage::Glsl420Core
        | OutputLanguage::Glsl430Core
        | OutputLanguage::Glsl440Core
        | OutputLanguage::Glsl450Core => {
            #[cfg(angle_enable_glsl)]
            output::glsl::generate(&mut ir, options);
            #[cfg(not(angle_enable_glsl))]
            panic!("Internal error: GLSL generator is not built");
        }
        OutputLanguage::Hlsl3 | OutputLanguage::Hlsl41 => {
            #[cfg(angle_enable_hlsl)]
            output::hlsl::generate(&mut ir, options);
            #[cfg(not(angle_enable_hlsl))]
            panic!("Internal error: HLSL generator is not built");
        }
        OutputLanguage::Spirv => {
            #[cfg(angle_enable_spirv)]
            output::spirv::generate(&mut ir, options);
            #[cfg(not(angle_enable_spirv))]
            panic!("Internal error: SPIR-V generator is not built");
        }
        OutputLanguage::Msl => {
            #[cfg(angle_enable_msl)]
            output::msl::generate(&mut ir, options);
            #[cfg(not(angle_enable_msl))]
            panic!("Internal error: MSL generator is not built");
        }
        OutputLanguage::Wgsl => {
            #[cfg(angle_enable_wgsl)]
            output::wgsl::generate(&mut ir, options);
            #[cfg(not(angle_enable_wgsl))]
            panic!("Internal error: WGSL generator is not built");
        }
        _ => panic!("Internal error: Invalid generator"),
    };

    // Run dead-code elimination again before generating output, so that any stray instructions the
    // transformations might have left around are removed.
    transform::run!(dead_code_eliminate, &mut ir);

    let reflection_info = ir.meta.take_reflection_info();

    // Passes required before AST can be generated:
    transform::run!(dealias, &mut ir);
    let astify_options = transform::astify::Options {
        max_expression_complexity: options.limits.max_expression_complexity,
    };
    let uncached_registers_with_side_effect = transform::run!(astify, &mut ir, &astify_options);

    let mut ast_gen = output::legacy::Generator::new(compiler, options);
    let mut generator = ast::Generator::new(*ir, uncached_registers_with_side_effect);
    let ast = generator.generate(&mut ast_gen);

    ffi::Output {
        ast,
        inputs: reflection_info.inputs,
        outputs: reflection_info.outputs,
        uniforms: reflection_info.uniforms,
        shared: reflection_info.shared,
        uniform_blocks: reflection_info.uniform_blocks,
        storage_blocks: reflection_info.storage_blocks,
    }
}

fn common_pre_variable_collection_transforms(ir: &mut IR, options: &Options) {
    // Turn |inout| variables that are never read from into |out| before collecting variables and
    // before PLS uses them.
    if ir.meta.get_shader_type() == ShaderType::Fragment
        && options.shader_version >= 300
        && (options.extensions.EXT_shader_framebuffer_fetch
            || options.extensions.EXT_shader_framebuffer_fetch_non_coherent)
    {
        transform::run!(remove_unused_framebuffer_fetch, ir);
    }

    // For now, rewrite pixel local storage before collecting variables or any operations on images.
    //
    // TODO(anglebug.com/40096838):
    //   Should this actually run after collecting variables?
    //   Do we need more introspection?
    //   Do we want to hide rewritten shader image uniforms from glGetActiveUniform?
    if options.rewrite_pixel_local_storage {
        // Remove PLS uniforms as function parameters to simplify transformations.  The rest of the
        // problematic args are handled later per output type.
        let transform_options = transform::monomorphize_unsupported_functions::Options {
            struct_containing_samplers: false,
            image: false,
            atomic_counter: false,
            array_of_array_of_sampler_or_image: false,
            pixel_local_storage: true,
        };
        transform::run!(monomorphize_unsupported_functions, ir, &transform_options);

        let transform_options = transform::rewrite_pixel_local_storage::Options {
            pls: options.pls_options,
            // PLS attachments are bound in reverse order from the rear.
            max_framebuffer_fetch_location: options
                .limits
                .max_combined_draw_buffers_and_pixel_local_storage_planes
                - 1,
            pass_highp_to_pack_unorm_snorm_built_ins: options
                .pass_highp_to_pack_unorm_snorm_built_ins,
        };
        transform::run!(rewrite_pixel_local_storage, ir, &transform_options);
    }

    if options.emulate_instanced_multiview
        && (options.extensions.OVR_multiview || options.extensions.OVR_multiview2)
    {
        let transform_options = transform::emulate_instanced_multiview::Options {
            shader_type: ir.meta.get_shader_type(),
            select_viewport_layer: options.select_viewport_layer_in_emulated_multiview,
        };
        transform::run!(emulate_instanced_multiview, ir, &transform_options);
    }

    let emulate_draw_id = options.emulate_draw_id && options.extensions.ANGLE_multi_draw;
    let emulate_base_vertex_instance = options.emulate_base_vertex_instance
        && options.extensions.ANGLE_base_vertex_base_instance_shader_builtin;
    if emulate_draw_id || emulate_base_vertex_instance {
        let transform_options = transform::emulate_multi_draw::Options {
            emulate_draw_id,
            emulate_base_vertex_instance,
            add_base_vertex_to_vertex_id: options.add_base_vertex_to_vertex_id,
        };
        transform::run!(emulate_multi_draw, ir, &transform_options);
    }

    if ir.meta.get_shader_type() == ShaderType::Fragment
        && options.shader_version == 100
        && options.extensions.EXT_draw_buffers
        && options.limits.max_draw_buffers > 1
    {
        let transform_options = transform::broadcast_fragcolor::Options {
            max_draw_buffers: options.limits.max_draw_buffers,
            max_dual_source_draw_buffers: options.limits.max_dual_source_draw_buffers,
        };
        transform::run!(broadcast_fragcolor, ir, &transform_options);
    }

    // Sort uniforms based on their precisions and data types for better packing.
    transform::run!(sort_uniforms, ir);
}

fn collect_reflection_info(ir: &mut IR, options: &Options) {
    // Basic dead-code-elimination to avoid outputting variables, constants and types that are not
    // used by the shader.
    //
    // This is done before collecting reflection info so that as many interface
    // variables can be detected as inactive.  However, reflection info for inactive variables must
    // also be collected, so the transformation reports the set of active interface variables
    // without removing inactive ones.
    let active_interface_variables = transform::run!(dead_code_eliminate, ir);

    {
        let reflection_options = reflection::Options {
            is_es1: options.shader_version == 100,
            transform_float_uniform_to_fp16: options.transform_float_uniform_to_fp16,
        };
        ir.collect_reflection_info(&reflection_options, &active_interface_variables);
    }

    // Now that reflection info is collected, inactive interface variables can be pruned.
    //
    // Note that inactive outputs must be retained.  Imagine a situation where the VS doesn't write
    // to a varying but the FS reads from it.  This is allowed, though the value of the varying is
    // undefined.  If the varying is removed here, the situation is changed to VS not declaring the
    // varying, but the FS reading from it, which is not allowed.  That's why inactive shader
    // outputs are not removed.  Inactive fragment shader outputs can be removed though.
    //
    // For now, inactive built-ins are also retained.
    if options.remove_inactive_interface_variables {
        let retain_inactive_outputs = options.retain_inactive_fragment_outputs
            || ir.meta.get_shader_type() != ShaderType::Fragment;

        ir.meta.prune_global_variables(|variable_id, variable| {
            // Keep the variable if:
            //
            // * Not an interface variable, or
            // * Is active, or
            // * Is output and should be kept, or
            // * Is built-in
            !variable.is_interface_variable()
                || active_interface_variables.contains(&variable_id)
                || (retain_inactive_outputs && variable.decorations.has(Decoration::Output))
                || variable.is_built_in()
        });
    }
}

fn common_post_variable_collection_transforms(ir: &mut IR, options: &Options) {
    // Run after unused variables are removed, initialize local and output variables if necessary.
    if options.initialize_uninitialized_variables {
        let transform_options = transform::initialize_uninitialized_variables::Options {
            loops_allowed_when_initializing_variables: options
                .loops_allowed_when_initializing_variables,
            initializer_allowed_on_non_constant_global_variables: options
                .initializer_allowed_on_non_constant_global_variables,
        };
        transform::run!(initialize_uninitialized_variables, ir, &transform_options);
    }

    if options.clamp_point_size {
        transform::run!(
            clamp_point_size,
            ir,
            options.limits.min_point_size,
            options.limits.max_point_size,
        );
    }

    if options.clamp_frag_depth {
        transform::run!(clamp_frag_depth, ir);
    }

    if options.scalarize_vec_and_mat_constructor_args {
        transform::run!(scalarize_vec_and_mat_constructor_args, ir);
    }

    if options.clamp_indirect_indices {
        let transform_options = transform::localized_workarounds::Options {
            clamp_indirect_indices: options.clamp_indirect_indices,
        };
        transform::run!(localized_workarounds, ir, &transform_options);
    }
}

fn initialize_global_pool_index_workaround() {
    unsafe { ffi::initialize_global_pool_index() };
}
fn free_global_pool_index_workaround() {
    unsafe { ffi::free_global_pool_index() };
}
