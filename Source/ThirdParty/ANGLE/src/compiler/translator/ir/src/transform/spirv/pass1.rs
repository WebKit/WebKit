// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A set of independent transformations that are applied in one pass.
//
// * vertex_instance_id: transforms `Load`, when the argument is `gl_InstanceIndex`.

use crate::ir::*;
use crate::*;

use crate::transform::add_driver_uniforms::DriverUniforms;
use crate::transform::spirv::vertex_instance_id;

pub struct Options<'a> {
    pub driver_uniforms: &'a DriverUniforms,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    vertex_instance_id_state: vertex_instance_id::State,
}

pub fn run(ir: &mut IR, options: &Options) {
    let vertex_instance_id_state = vertex_instance_id::init(&mut ir.meta);
    let mut state = State { ir_meta: &mut ir.meta, vertex_instance_id_state };

    traverser::transformer::for_each_instruction(
        &mut state,
        &mut ir.function_entries,
        &|state, instruction| {
            let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
            match *opcode {
                OpCode::Load(pointer) => transform_load(state, options, pointer, result.unwrap()),
                _ => traverser::Transforms::new(),
            }
        },
    );
}

fn transform_load(
    state: &mut State,
    options: &Options,
    pointer: TypedId,
    result: TypedRegisterId,
) -> traverser::Transforms {
    if let Id::Variable(variable_id) = pointer.id {
        let variable = state.ir_meta.get_variable(variable_id);
        if matches!(variable.built_in, Some(BuiltIn::InstanceIndex)) {
            vertex_instance_id::transform_instance_index_load(
                state.ir_meta,
                &state.vertex_instance_id_state,
                options.driver_uniforms,
                pointer,
                result,
            )
        } else {
            traverser::Transforms::new()
        }
    } else {
        traverser::Transforms::new()
    }
}
