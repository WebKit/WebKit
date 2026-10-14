// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ir::*;
use crate::*;

pub fn generate(ir: &mut IR, options: &compile::Options) {
    if options.ensure_loop_forward_progress {
        transform::msl::ensure_loop_forward_progress::run(ir);
    }

    {
        let transform_options = transform::monomorphize_unsupported_functions::Options {
            struct_containing_samplers: true,
            // ESSL 310+ are not supported.
            image: false,
            atomic_counter: false,
            array_of_array_of_sampler_or_image: false,
            // Already done by common code.
            pixel_local_storage: false,
        };
        transform::run!(monomorphize_unsupported_functions, ir, &transform_options);
    }

    transform::run!(rewrite_struct_samplers, ir);
    transform::run!(rewrite_array_of_array_of_opaque_uniforms, ir);

    {
        let transform_options = transform::add_driver_uniforms::Options {
            declare_as_struct: true,
            add_coverage_mask: true,
            add_xfb_emulation: true,
            // Only needed for SPIR-V
            add_transform_xy: false,
        };
        transform::run!(add_driver_uniforms, ir, &transform_options);
    }
}
