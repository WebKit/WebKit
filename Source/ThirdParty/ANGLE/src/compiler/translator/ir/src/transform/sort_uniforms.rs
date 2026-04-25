// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Sort uniforms for better packing.
use crate::ir::*;

const SORT_LAST_RANKING: u32 = 1000000;
const ARRAY_RANKING_OFFSET: u32 = 100;

pub fn run(ir: &mut IR) {
    // The global variables are sorted such that uniforms are placed first.  Within the uniforms,
    // those with a non-high precision are placed first.  Within each group, the uniforms are
    // sorted as follows:
    //
    // - float
    // - vec2
    // - vec3
    // - vec4
    // - mat2x2
    // - mat2x3
    // - mat2x4
    // - mat3x2
    // - mat3x3
    // - mat3x4
    // - mat4x2
    // - mat4x3
    // - mat4x4
    // - Other scalar
    // - Other vector
    // - float[]
    // - vec*[] (with the same suborder as above)
    // - mat*[] (with the same suborder as above)
    // - Other scalar array
    // - Other vector array
    // - Structs and Opaque types
    //
    // This sorting is done with the help of a map that assigns a ranking to (pointers to) each of
    // the above types first.  During sorting, the ranking is looked up and comparison is made.
    let mut ranking = vec![SORT_LAST_RANKING; ir.meta.all_types().len()];

    // Hand-craft the ranking for basic types.  Variables are not of these types though, they are
    // pointers to these types, and those are assigned afterwards.
    ranking[TYPE_ID_FLOAT.id as usize] = 0;

    ranking[TYPE_ID_VEC2.id as usize] = 1;
    ranking[TYPE_ID_VEC3.id as usize] = 2;
    ranking[TYPE_ID_VEC4.id as usize] = 3;

    ranking[TYPE_ID_MAT2.id as usize] = 4;
    ranking[TYPE_ID_MAT2X3.id as usize] = 5;
    ranking[TYPE_ID_MAT2X4.id as usize] = 6;

    ranking[TYPE_ID_MAT3X2.id as usize] = 7;
    ranking[TYPE_ID_MAT3.id as usize] = 8;
    ranking[TYPE_ID_MAT3X4.id as usize] = 9;

    ranking[TYPE_ID_MAT4X2.id as usize] = 10;
    ranking[TYPE_ID_MAT4X3.id as usize] = 11;
    ranking[TYPE_ID_MAT4.id as usize] = 12;

    ranking[TYPE_ID_INT.id as usize] = 13;
    ranking[TYPE_ID_UINT.id as usize] = 14;
    ranking[TYPE_ID_BOOL.id as usize] = 15;

    ranking[TYPE_ID_IVEC2.id as usize] = 16;
    ranking[TYPE_ID_UVEC2.id as usize] = 17;
    ranking[TYPE_ID_BVEC2.id as usize] = 18;

    ranking[TYPE_ID_IVEC3.id as usize] = 19;
    ranking[TYPE_ID_UVEC3.id as usize] = 20;
    ranking[TYPE_ID_BVEC3.id as usize] = 21;

    ranking[TYPE_ID_IVEC4.id as usize] = 22;
    ranking[TYPE_ID_UVEC4.id as usize] = 23;
    ranking[TYPE_ID_BVEC4.id as usize] = 24;

    const { assert!(ARRAY_RANKING_OFFSET > 24) }

    let (mut to_sort, mut rest): (Vec<_>, Vec<_>) =
        ir.meta.all_global_variables().iter().partition(|&id| {
            let variable = ir.meta.get_variable(*id);
            variable.decorations.has(Decoration::Uniform)
        });

    // Assign ranking to the variable types.
    to_sort.iter().for_each(|&id| {
        let type_id = ir.meta.get_variable(id).type_id;
        let pointee_type_id = ir.meta.get_pointee_type(type_id);
        let rank = get_base_ranking(&ir.meta, pointee_type_id, &ranking);
        ranking[type_id.id as usize] = rank;
    });

    to_sort.sort_by(|&a, &b| {
        let a = ir.meta.get_variable(a);
        let b = ir.meta.get_variable(b);
        let a_is_highp = a.precision == Precision::High;
        let b_is_highp = b.precision == Precision::High;
        a_is_highp
            .cmp(&b_is_highp)
            .then(ranking[a.type_id.id as usize].cmp(&ranking[b.type_id.id as usize]))
    });
    to_sort.append(&mut rest);
    ir.meta.replace_global_variables(to_sort);
}

fn get_base_ranking(ir_meta: &IRMeta, type_id: TypeId, ranking: &[u32]) -> u32 {
    if ranking[type_id.id as usize] < SORT_LAST_RANKING {
        // Rank is already determined
        return ranking[type_id.id as usize];
    }

    let type_info = ir_meta.get_type(type_id);
    match *type_info {
        // Place opaque uniforms and structs last
        Type::Image(..) | Type::Scalar(BasicType::AtomicCounter) | Type::Struct(..) => {
            SORT_LAST_RANKING
        }
        // For arrays, place them after non-arrays, but otherwise in a similar order.  This is
        // achieved by offsetting their ranking.
        Type::Array(element_type_id, _) => {
            get_base_ranking(ir_meta, element_type_id, ranking) + ARRAY_RANKING_OFFSET
        }
        // Every other type should either be invalid for a variable or already ranked.
        _ => panic!("Internal error: Variable type should have already been ranked"),
    }
}
