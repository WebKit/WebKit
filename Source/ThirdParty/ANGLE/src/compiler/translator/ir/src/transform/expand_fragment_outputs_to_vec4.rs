// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Turn gvecN (N < 4) fragment outputs to gvec4, filling the missing components with 0.

use crate::ir::*;
use crate::*;

pub fn run(ir: &mut IR) {
    for variable_id in ir.meta.all_global_variables().clone() {
        // Must be an `out` variable
        let variable = ir.meta.get_variable(variable_id);
        if !variable.decorations.has(Decoration::Output) {
            continue;
        }

        // Must have fewer than 4 components.
        let base_type_id = ir.meta.get_pointee_type(variable.type_id);
        let (base_type_id, array_size) =
            if let &Type::Array(base_type_id, size) = ir.meta.get_type(base_type_id) {
                (base_type_id, Some(size))
            } else {
                (base_type_id, None)
            };

        if let Some((expanded_type_id, component_count, zero)) = expand_type_to_vec4(base_type_id) {
            replace_output_variable(
                ir,
                variable_id,
                variable.name.name,
                expanded_type_id,
                component_count,
                array_size,
                zero,
            );
        }
    }
}

fn expand_type_to_vec4(type_id: TypeId) -> Option<(TypeId, u32, TypedId)> {
    match type_id {
        TYPE_ID_FLOAT => Some((TYPE_ID_VEC4, 1, TYPED_CONSTANT_ID_FLOAT_ZERO)),
        TYPE_ID_VEC2 => Some((TYPE_ID_VEC4, 2, TYPED_CONSTANT_ID_FLOAT_ZERO)),
        TYPE_ID_VEC3 => Some((TYPE_ID_VEC4, 3, TYPED_CONSTANT_ID_FLOAT_ZERO)),
        TYPE_ID_INT => Some((TYPE_ID_IVEC4, 1, TYPED_CONSTANT_ID_INT_ZERO)),
        TYPE_ID_IVEC2 => Some((TYPE_ID_IVEC4, 2, TYPED_CONSTANT_ID_INT_ZERO)),
        TYPE_ID_IVEC3 => Some((TYPE_ID_IVEC4, 3, TYPED_CONSTANT_ID_INT_ZERO)),
        TYPE_ID_UINT => Some((TYPE_ID_UVEC4, 1, TYPED_CONSTANT_ID_UINT_ZERO)),
        TYPE_ID_UVEC2 => Some((TYPE_ID_UVEC4, 2, TYPED_CONSTANT_ID_UINT_ZERO)),
        TYPE_ID_UVEC3 => Some((TYPE_ID_UVEC4, 3, TYPED_CONSTANT_ID_UINT_ZERO)),
        _ => None,
    }
}

fn replace_output_variable(
    ir: &mut IR,
    variable_id: VariableId,
    name: &'static str,
    expanded_type_id: TypeId,
    component_count: u32,
    array_size: Option<u32>,
    basic_type_zero: TypedId,
) {
    let complete_type = if let Some(array_size) = array_size {
        ir.meta.get_array_type_id(expanded_type_id, array_size)
    } else {
        expanded_type_id
    };

    // Use a global instead of the fragment output, declare a gvec4 fragment output instead and
    // and at the end of the shader copy it over to the expanded variable.
    let new_output =
        ir.meta.declare_cached_global_for_variable(variable_id, name, Some(complete_type)).1;
    let global = TypedId::from_variable_id(&ir.meta, variable_id);

    let mut postamble = Block::new();
    let mut builder = instruction::Builder::<&mut Block>::new(&mut postamble);

    if let Some(array_size) = array_size {
        for index in 0..array_size {
            // globalN = AccessArrayElement global N
            // outputN = AccessArrayElement output N
            let index = ir.meta.get_constant_uint_typed(index, Precision::Low);
            let global_indexed = builder.index(&mut ir.meta, global, index);
            let output_indexed = builder.index(&mut ir.meta, new_output, index);
            copy_and_expand(
                &mut ir.meta,
                &mut builder,
                global_indexed,
                output_indexed,
                expanded_type_id,
                component_count,
                basic_type_zero,
            );
        }
    } else {
        copy_and_expand(
            &mut ir.meta,
            &mut builder,
            global,
            new_output,
            expanded_type_id,
            component_count,
            basic_type_zero,
        );
    }

    ir.append_to_main(postamble);
}

fn copy_and_expand(
    ir_meta: &mut IRMeta,
    builder: &mut instruction::Builder<&mut Block>,
    global: TypedId,
    output: TypedId,
    expanded_type_id: TypeId,
    component_count: u32,
    basic_type_zero: TypedId,
) {
    // Produce statements like output = vec4(global.x, global.y, zero, zero).
    let mut constructor_args = if component_count > 1 {
        // x        = AccessVectorComponent global 0
        // x'       = Load x
        // y        = AccessVectorComponent global 1
        // y'       = Load y
        // expanded = ConstructVectorFromMultiple (x, y, zero, zero)
        (0..component_count)
            .map(|component| {
                let swizzle = builder.vector_component(ir_meta, global, component);
                builder.load(ir_meta, swizzle)
            })
            .collect::<Vec<_>>()
    } else {
        // x        = Load global
        // expanded = ConstructVectorFromMultiple (x, zero, zero, zero)
        vec![builder.load(ir_meta, global)]
    };
    constructor_args.resize(4, basic_type_zero);
    let expanded = builder.construct(ir_meta, expanded_type_id, constructor_args, None);
    builder.store(ir_meta, output, expanded);
}
