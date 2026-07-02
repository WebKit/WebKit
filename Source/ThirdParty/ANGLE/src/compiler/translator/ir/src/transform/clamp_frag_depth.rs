// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Clamp gl_FragDepth to [0, 1].
//
use crate::ir::*;
use crate::*;

pub fn run(ir: &mut IR) {
    if let Some(frag_depth_var_id) = ir.meta.get_built_in_variable(BuiltIn::FragDepth) {
        let frag_depth = TypedId::from_variable_id(&ir.meta, frag_depth_var_id);

        // Generate:
        //
        //     depth   = Load frag_depth
        //     clamped = Clamp depth min max
        //               Store frag_depth clamped
        let mut clamp_block = Block::new();
        let depth = clamp_block.add_typed_instruction(instruction::load(&mut ir.meta, frag_depth));
        let clamped = clamp_block.add_typed_instruction(instruction::built_in(
            &mut ir.meta,
            BuiltInOpCode::Clamp,
            vec![depth, TYPED_CONSTANT_ID_FLOAT_ZERO, TYPED_CONSTANT_ID_FLOAT_ONE],
        ));
        clamp_block.add_void_instruction(OpCode::Store(frag_depth, clamped));

        ir.append_to_main(clamp_block);
    }
}
