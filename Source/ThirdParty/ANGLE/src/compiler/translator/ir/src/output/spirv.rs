// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ir::*;
use crate::*;

pub fn generate(ir: &mut IR, options: &compile::Options) {
    {
        let transform_options = transform::monomorphize_unsupported_functions::Options {
            struct_containing_samplers: true,
            image: options.shader_version >= 310,
            atomic_counter: options.shader_version >= 310,
            array_of_array_of_sampler_or_image: options.shader_version >= 310,
            // Already done by common code.
            pixel_local_storage: false,
        };
        transform::run!(monomorphize_unsupported_functions, ir, &transform_options);
    }

    transform::run!(rewrite_struct_samplers, ir);
    transform::run!(rewrite_array_of_array_of_opaque_uniforms, ir);

    let driver_uniforms = {
        let transform_options = transform::add_driver_uniforms::Options {
            // Always declared for SPIR-V for simplicity, though used only with the
            // prefer_precomputed_vertex_transform option.
            add_transform_xy: true,
            // Only declared if transform feedback needs emulation
            add_xfb_emulation: options.add_vulkan_xfb_emulation_support_code,
            // Only needed for MSL
            declare_as_struct: false,
            add_coverage_mask: false,
        };
        transform::run!(add_driver_uniforms, ir, &transform_options)
    };

    {
        let transform_options =
            transform::spirv::pass1::Options { driver_uniforms: &driver_uniforms };
        transform::run!(spirv::pass1, ir, &transform_options);
    }
}
