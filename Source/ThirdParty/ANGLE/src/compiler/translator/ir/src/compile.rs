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

    // TODO(http://anglebug.com/349994211): equivalent to ShBuiltInResources, or at least the parts that the compiler really uses:
    // add as needed.

    // Matching ShShaderOutput
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum OutputLanguage {
        Null,
        Essl,
        GlslCompatibility,
        Glsl130,
        Glsl140,
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

    struct CompileOptions {
        // Input shader and device properties:

        // The version of the input version
        shader_version: i32,
        // Extensions that were enabled, mostly useful for the GLSL/ESSL output to replicate them.
        extensions: ExtensionsEnabled,
        // TODO(http://anglebug.com/349994211): equivalent to ShBuiltInResources

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
        // TODO(http://anglebug.com/349994211): equivalent to ShCompileOptions flags
    }

    // TODO(http://anglebug.com/349994211): Equivalent to sh::ShaderVariable, to be done after
    // CollectVariables is implemented in the IR.
    struct ShaderVariable {
        todo: i32,
    }

    struct Output {
        // Note: For now, generate always results in an AST. Eventually either text or binary
        // should be returned.
        ast: *mut TIntermBlock,
        // TODO(http://anglebug.com/349994211): Reflection data collected by the IR (equivalent to
        // what CollectVariables produces).
        variables: Vec<ShaderVariable>,
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

pub use ffi::CompileOptions as Options;
pub use ffi::OutputLanguage;

unsafe fn generate_ast(
    mut ir: Box<IR>,
    compiler: *mut ffi::TCompiler,
    allocator: *mut ffi::PoolAllocator,
    options: &Options,
) -> ffi::Output {
    unsafe { ffi::set_global_pool_allocator(allocator) };

    // Apply transforms shared by multiple generators:
    common_pre_variable_collection_transforms(&mut ir, options);
    // TODO(http://anglebug.com/349994211): Run variable collection (reflection info) in between
    // these two transforms.
    common_post_variable_collection_transforms(&mut ir, options);

    // Passes required before AST can be generated:
    transform::dealias::run(&mut ir);
    transform::astify::run(&mut ir);

    let mut ast_gen = output::legacy::Generator::new(compiler, options);
    let mut generator = ast::Generator::new(*ir);
    let ast = generator.generate(&mut ast_gen);

    ffi::Output { ast, variables: vec![] }
}

fn common_pre_variable_collection_transforms(ir: &mut IR, options: &Options) {
    // Turn |inout| variables that are never read from into |out| before collecting variables and
    // before PLS uses them.
    if ir.meta.get_shader_type() == ShaderType::Fragment
        && options.shader_version >= 300
        && (options.extensions.EXT_shader_framebuffer_fetch
            || options.extensions.EXT_shader_framebuffer_fetch_non_coherent)
    {
        transform::remove_unused_framebuffer_fetch::run(ir);
    }
}

fn common_post_variable_collection_transforms(ir: &mut IR, options: &Options) {
    // Basic dead-code-elimination to avoid outputting variables, constants and types that are not
    // used by the shader.
    transform::prune_unused_variables::run(ir);

    // Run after unused variables are removed, initialize local and output variables if necessary.
    if options.initialize_uninitialized_variables {
        let transform_options = transform::initialize_uninitialized_variables::Options {
            loops_allowed_when_initializing_variables: options
                .loops_allowed_when_initializing_variables,
            initializer_allowed_on_non_constant_global_variables: options
                .initializer_allowed_on_non_constant_global_variables,
        };
        transform::initialize_uninitialized_variables::run(ir, &transform_options);
    }

    // Note: this is a per-generator transformation, not really "common", so it should be moved to
    // the right section when the IR part of the compilation actually starts to deviate per
    // generator.  For now, this is run to test the transformation, with a future change calling it
    // earlier (pre variable collection) for Pixel Local Storage, and later for the rest of the
    // options.
    {
        let transform_options = transform::monomorphize_unsupported_functions::Options {
            // Samplers in structs are unsupported by most generators.
            // TODO(http://anglebug.com/349994211): The HLSL generator should also take advantage
            // of this transformation, instead of dealing with samplers-in-structs independently.
            struct_containing_samplers: matches!(
                options.output,
                OutputLanguage::Spirv | OutputLanguage::Msl | OutputLanguage::Wgsl
            ),
            // http://anglebug.com/42265954: The ESSL spec has a bug with images as function
            // arguments. The recommended workaround is to inline functions that accept image
            // arguments.
            image: options.shader_version >= 310,
            atomic_counter: options.shader_version >= 310
                && matches!(options.output, OutputLanguage::Spirv),
            array_of_array_of_sampler_or_image: options.shader_version >= 310
                && matches!(options.output, OutputLanguage::Spirv),
            pixel_local_storage: false,
        };
        transform::monomorphize_unsupported_functions::run(ir, &transform_options);
    }
}

fn initialize_global_pool_index_workaround() {
    unsafe { ffi::initialize_global_pool_index() };
}
fn free_global_pool_index_workaround() {
    unsafe { ffi::free_global_pool_index() };
}
