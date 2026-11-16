// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// * Change gl_VertexID to gl_VertexIndex
// * Change gl_InstanceID to gl_InstanceID - angle_BaseInstance

use crate::ir::*;
use crate::*;

use crate::transform::add_driver_uniforms::DriverUniforms;

pub struct State {}

pub fn init(ir_meta: &mut IRMeta) -> State {
    // If gl_VertexID or gl_InstanceID are defined, replace them with gl_VertexIndex and
    // gl_InstanceIndex.  They match in type and everything, so simply switching the BuiltIn value
    // is enough.
    if let Some(vertex_id) = ir_meta.get_built_in_variable(BuiltIn::VertexID) {
        ir_meta.get_variable_mut(vertex_id).built_in = Some(BuiltIn::VertexIndex);
    }
    if let Some(instance_id) = ir_meta.get_built_in_variable(BuiltIn::InstanceID) {
        ir_meta.get_variable_mut(instance_id).built_in = Some(BuiltIn::InstanceIndex);
    }

    State {}
}

pub fn transform_instance_index_load(
    ir_meta: &mut IRMeta,
    _state: &State,
    driver_uniforms: &DriverUniforms,
    instance_index: TypedId,
    result: TypedRegisterId,
) -> traverser::Transforms {
    debug_assert!(matches!(
        ir_meta.get_variable(instance_index.id.get_variable()).built_in,
        Some(BuiltIn::InstanceIndex)
    ));

    let mut builder = instruction::Builder::<traverser::Transforms>::new();

    // Generate the following:
    //
    //     base_instance'   = Load base_instance
    //     instance_index'  = Load instance_index
    //     result           = Sub instance_index' base_instance'
    let base_instance = driver_uniforms.base_instance(ir_meta, &mut builder);
    let instance_index = builder.load(ir_meta, instance_index);
    let instance_index = builder.sub(ir_meta, instance_index, base_instance);

    builder.finish_with_result(ir_meta, result, instance_index)
}
