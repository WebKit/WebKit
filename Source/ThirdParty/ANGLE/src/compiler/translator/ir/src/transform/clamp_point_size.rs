// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Limit the value that is written to gl_PointSize.
//
use crate::ir::*;
use crate::*;

pub fn run(ir: &mut IR, min_point_size: f32, max_point_size: f32) {
    if let Some(point_size_var_id) = ir.meta.get_built_in_variable(BuiltIn::PointSize) {
        let point_size = TypedId::from_variable_id(&ir.meta, point_size_var_id);
        let min_size_constant = ir.meta.get_constant_float_typed(min_point_size);
        let max_size_constant = ir.meta.get_constant_float_typed(max_point_size);

        let mut clamp_block = Block::new();
        let point_size_source =
            clamp_block.add_typed_instruction(instruction::load(&mut ir.meta, point_size));
        let clamped = clamp_block.add_typed_instruction(instruction::built_in(
            &mut ir.meta,
            BuiltInOpCode::Clamp,
            vec![point_size_source, min_size_constant, max_size_constant],
        ));
        clamp_block.add_void_instruction(OpCode::Store(point_size, clamped));

        ir.append_to_main(clamp_block);
    }
}
