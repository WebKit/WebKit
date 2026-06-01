// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// * Change gl_VertexID to gl_VertexIndex
// * Change gl_InstanceID to gl_InstanceID - angle_BaseInstance

use crate::ir::*;
use crate::*;

pub struct State {
    base_instance: Option<TypedId>,
}

pub fn init(ir_meta: &mut IRMeta) -> State {
    // If gl_VertexID or gl_InstanceID are defined, replace them with gl_VertexIndex and
    // gl_InstanceIndex.  They match in type and everything, so simply switching the BuiltIn value
    // is enough.
    if let Some(vertex_id) = ir_meta.get_built_in_variable(BuiltIn::VertexID) {
        ir_meta.get_variable_mut(vertex_id).built_in = Some(BuiltIn::VertexIndex);
    }
    if let Some(instance_id) = ir_meta.get_built_in_variable(BuiltIn::InstanceID) {
        ir_meta.get_variable_mut(instance_id).built_in = Some(BuiltIn::InstanceIndex);

        // If angle_BaseInstance is defined, find it.
        let base_instance = ir_meta
            .all_global_variables()
            .iter()
            .find(|&id| {
                ir_meta
                    .get_variable(*id)
                    .decorations
                    .has(Decoration::EmulatedMultiDrawBuiltIn(EmulatedMultiDraw::BaseInstance))
            })
            .map(|&id| TypedId::from_variable_id(ir_meta, id));

        State { base_instance }
    } else {
        State { base_instance: None }
    }
}

pub fn transform_instance_index_load(
    ir_meta: &mut IRMeta,
    state: &State,
    instance_index: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    debug_assert!(matches!(
        ir_meta.get_variable(instance_index.id.get_variable()).built_in,
        Some(BuiltIn::InstanceIndex)
    ));

    let mut transforms = vec![];

    if let Some(base_instance) = state.base_instance {
        // Generate the following:
        //
        //     base_instance'   = Load base_instance
        //     instance_index'  = Load instance_index
        //     result           = Sub instance_index' base_instance'
        let base_instance = traverser::add_typed_instruction(
            &mut transforms,
            instruction::make!(load, ir_meta, base_instance),
        );
        let instance_index = traverser::add_typed_instruction(
            &mut transforms,
            instruction::make!(load, ir_meta, instance_index),
        );
        traverser::add_typed_instruction(
            &mut transforms,
            instruction::make_with_result_id!(sub, ir_meta, result, instance_index, base_instance),
        );
    }

    transforms
}
