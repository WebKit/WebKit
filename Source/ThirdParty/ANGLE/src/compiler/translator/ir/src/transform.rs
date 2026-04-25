// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
pub mod astify;
pub mod broadcast_fragcolor;
pub mod clamp_frag_depth;
pub mod clamp_point_size;
pub mod dead_code_eliminate;
pub mod dealias;
pub mod emulate_instanced_multiview;
pub mod emulate_multi_draw;
pub mod initialize_uninitialized_variables;
pub mod localized_workarounds;
pub mod monomorphize_unsupported_functions;
pub mod propagate_precision;
pub mod remove_unused_framebuffer_fetch;
pub mod rewrite_pixel_local_storage;
pub mod scalarize_vec_and_mat_constructor_args;
pub mod sort_uniforms;

// Transformations that only a single code generator uses.
#[cfg(angle_enable_essl)]
pub mod essl {}
#[cfg(angle_enable_glsl)]
pub mod glsl {}
#[cfg(any(angle_enable_essl, angle_enable_glsl))]
pub mod glsl_common {}
#[cfg(angle_enable_hlsl)]
pub mod hlsl {}
#[cfg(angle_enable_msl)]
pub mod msl {
    pub mod ensure_loop_forward_progress;
}
#[cfg(angle_enable_spirv)]
pub mod spirv {
    pub mod pass1;
    mod vertex_instance_id;
}
#[cfg(angle_enable_wgsl)]
pub mod wgsl {}

// Helper macro to run a transformation and automatically validate the IR afterwards
macro_rules! run {
    ($func:ident $(::$path:ident)*, $ir:expr $(, $params:expr)*$(,)*) => {
        {
            let result = $crate::transform::$func$(::$path)*::run($ir $(, $params)*);
            $crate::ir::validate!($ir);
            result
        }
    };
}
pub(crate) use run;
