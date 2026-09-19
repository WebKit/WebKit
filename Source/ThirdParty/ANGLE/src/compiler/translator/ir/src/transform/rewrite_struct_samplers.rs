// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Extract samplers out of structs and turn them into independent uniforms.
//
// # General algorithm:
//
// Take these structs, note that the parser puts samplers and sampler-only structs last.
//
//     struct OnlySamplers {
//        sampler2D s;
//     };
//
//     struct Mixed {
//        float f;
//        sampler2D s[2];
//        samplerCube s2[3];
//     };
//
//     uniform struct S {
//        float f;
//        Mixed m[3];
//        float f2;
//        OnlySamplers s[4][5];
//     } u;
//
// First, some information is gathered for each struct for easy look up:
//
//  * OnlySamplers:
//    * Field 0: is sampler and only sampler, sampler2D, arrayness: []
//      * first field with sampler
//      * first sampler-only field
//  * Mixed:
//    * Field 0: not sampler
//    * Field 1: is sampler and only sampler, sampler2D, arrayness: [2]
//      * first field with sampler
//      * first sampler-only field
//    * Field 2: is sampler and only sampler, samplerCube, arrayness: [3]
//  * S:
//    * Field 0: not sampler
//    * Field 1: is sampler but also non-sampler, Mixed, arrayness: [3]
//      * first field with sampler
//    * Field 2: not sampler
//    * Field 3: is sampler and only sampler, OnlySamplers, arrayness: [4, 5]
//      * first sampler-only field
//
// Then, for each uniform of struct-with-sampler type, the samplers are extracted as standalone
// uniforms and removed from struct fields, producing the following.  In the meantime, a map is
// created, mapping `u.<field0>.<field1>...` to `extractedSampler_N`.
//
//     struct Mixed {
//        float f;
//     };
//
//     uniform struct S {
//        float f;
//        Mixed m[3];
//        float f2;
//     } u;
//
//     uniform sampler2D extractedSampler_0[3][2];
//     uniform samplerCube extractedSampler_1[3][3];
//     uniform sampler2D extractedSampler_2[4][5];
//
// Then, the instructions are walked to fix up accesses to the samplers.  Starting from
// `AccessArrayElement` and `AccessStructField` on uniform variables, the fields and indices applied
// to the pointers are tracked if they might lead to a sampler in a struct.  Once a sampler is
// accessed and a tracked access leads to it, it must be a sampler in struct, in which case the
// `u.<field0>....` path is looked up to find `extractedSampler_N`, the array indices are applied to
// it to replace the access chain.
//
// Ultimately, the access chains are used in a Load or Call instruction.  In
// the monomorphize_unsupported_functions pass, sampler array parameters are eliminated if passed a
// sampler-in-struct, so only the access chain leading to a sampler (not array) needs replacement.

use crate::ir::*;
use crate::*;

#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
struct FieldInfo {
    has_sampler: bool,
    has_only_samplers: bool,
    base_type_id: TypeId,
    arrayness: Vec<u32>,
    precision: Precision,
}

// For each struct type, caches which fields have samplers
#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
struct StructInfo {
    field_info: Vec<FieldInfo>,
    has_sampler: bool,
    first_sampler_field: usize,
    first_sampler_only_field: usize,
}

impl StructInfo {
    fn new() -> StructInfo {
        StructInfo {
            field_info: vec![],
            has_sampler: false,
            first_sampler_field: 0,
            first_sampler_only_field: 0,
        }
    }
}

#[derive(Eq, PartialEq, Hash, Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
struct PathToSampler {
    uniform_var: VariableId,
    fields: Vec<u32>,
}

// While traversing the instructions, keep track of which instructions access uniforms and build up
// the access chains.  Because some fields are removed from structs, the instructions that are no
// longer valid are removed too.  Because of this, util::trace_back cannot be used as some of the
// instructions leading to the Load instruction are removed.
#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
struct AccessChain {
    path: PathToSampler,
    indices: Vec<TypedId>,
    accesses_sampler_only_field: bool,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    struct_info: Vec<StructInfo>,
    extracted_sampler_map: HashMap<PathToSampler, TypedId>,
    access_chains: Vec<Option<AccessChain>>,
}

pub fn run(ir: &mut IR) {
    let struct_info = gather_struct_info(&mut ir.meta);
    if struct_info.is_empty() {
        return;
    }

    let extracted_sampler_map = declare_extracted_samplers(&mut ir.meta, &struct_info);
    let access_chains = vec![None; ir.meta.total_register_count() as usize];

    let mut state =
        State { ir_meta: &mut ir.meta, struct_info, extracted_sampler_map, access_chains };

    traverser::transformer::for_each_instruction(
        &mut state,
        &mut ir.function_entries,
        &|state, instruction| {
            let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
            match *opcode {
                OpCode::AccessStructField(pointer, field) => {
                    transform_struct_field(state, pointer, field, result.unwrap())
                }
                OpCode::AccessArrayElement(pointer, index) => {
                    transform_array_element(state, pointer, index, result.unwrap())
                }
                _ => vec![],
            }
        },
    );
}

fn gather_struct_info(ir_meta: &mut IRMeta) -> Vec<StructInfo> {
    let mut struct_info = vec![StructInfo::new(); ir_meta.all_types().len()];
    let mut any_structs_with_samplers = false;

    // Gather info about which struct fields have samplers
    ir_meta.all_types().iter().enumerate().skip(MAX_PREDEFINED_TYPE_ID as usize).for_each(
        |(id, type_info)| {
            if let Type::Struct(_, fields, StructSpecialization::Struct) = type_info {
                let field_info = fields
                    .iter()
                    .map(|field| {
                        gather_field_info(
                            ir_meta,
                            &struct_info,
                            field.type_id,
                            field.precision,
                            vec![],
                        )
                    })
                    .collect::<Vec<_>>();
                // Record which struct has samplers, and where the sampler-only fields start.  Note
                // that the parser places samplers and structs that only contain samplers last.
                let first_sampler_field = field_info.iter().position(|info| info.has_sampler);
                let first_sampler_only_field =
                    field_info.iter().position(|info| info.has_only_samplers);

                let info = &mut struct_info[id];
                info.has_sampler = first_sampler_field.is_some();
                info.first_sampler_field = first_sampler_field.unwrap_or(field_info.len());
                info.first_sampler_only_field =
                    first_sampler_only_field.unwrap_or(field_info.len());
                info.field_info = field_info;

                if info.has_sampler {
                    any_structs_with_samplers = true;
                }
            }
        },
    );

    if !any_structs_with_samplers {
        return vec![];
    }

    // Strip samplers from the structs. Instructions that reference these fields will be removed
    // during transformation.
    struct_info.iter().enumerate().skip(MAX_PREDEFINED_TYPE_ID as usize).for_each(
        |(id, struct_info)| {
            if struct_info.has_sampler {
                let id = TypeId { id: id as u32 };
                if struct_info.first_sampler_only_field == 0 {
                    // If it's entirely made of samplers, eliminate the struct altogether.
                    ir_meta.dead_code_eliminate_type(id);
                } else {
                    // Otherwise, just reduce the number of fields.
                    match ir_meta.get_type_mut(id) {
                        Type::Struct(_, fields, StructSpecialization::Struct) => {
                            fields.truncate(struct_info.first_sampler_only_field);
                        }
                        _ => panic!(
                            "Internal error: Unexpected non-struct type marked as having samplers"
                        ),
                    };
                }
            }
        },
    );

    struct_info
}

fn gather_field_info(
    ir_meta: &IRMeta,
    struct_info: &Vec<StructInfo>,
    field_type_id: TypeId,
    precision: Precision,
    mut arrayness: Vec<u32>,
) -> FieldInfo {
    let field_type_info = ir_meta.get_type(field_type_id);

    if field_type_info.is_sampled_image() {
        FieldInfo {
            has_sampler: true,
            has_only_samplers: true,
            base_type_id: field_type_id,
            arrayness,
            precision,
        }
    } else {
        match *field_type_info {
            Type::Array(element_type_id, size) => {
                arrayness.push(size);
                gather_field_info(ir_meta, struct_info, element_type_id, precision, arrayness)
            }
            Type::Struct(..) => FieldInfo {
                has_sampler: struct_info[field_type_id.id as usize].has_sampler,
                has_only_samplers: struct_info[field_type_id.id as usize].first_sampler_only_field
                    == 0,
                base_type_id: field_type_id,
                arrayness,
                precision: Precision::NotApplicable,
            },
            _ => FieldInfo {
                has_sampler: false,
                has_only_samplers: false,
                base_type_id: field_type_id,
                arrayness,
                precision: Precision::NotApplicable,
            },
        }
    }
}

// Given `uniform type var[N][M]`, returns `type`
fn get_variable_base_type(ir_meta: &IRMeta, id: VariableId) -> TypeId {
    let type_id = ir_meta.get_pointee_type(ir_meta.get_variable(id).type_id);
    ir_meta.get_base_element_type(type_id)
}

// Given `uniform type var[N][M]`, returns `(type, [N, M])`
fn get_variable_base_type_and_arrayness(ir_meta: &IRMeta, id: VariableId) -> (TypeId, Vec<u32>) {
    let mut type_id = ir_meta.get_pointee_type(ir_meta.get_variable(id).type_id);
    let mut arrayness = vec![];

    while let Type::Array(element_id, size) = *ir_meta.get_type(type_id) {
        arrayness.push(size);
        type_id = element_id;
    }

    (type_id, arrayness)
}

fn declare_extracted_samplers(
    ir_meta: &mut IRMeta,
    struct_info: &Vec<StructInfo>,
) -> HashMap<PathToSampler, TypedId> {
    let mut next_sampler_index = 0;
    let mut extracted_sampler_map = HashMap::new();

    let uniforms = ir_meta
        .all_global_variables()
        .iter()
        .filter(|&variable_id| {
            ir_meta.get_variable(*variable_id).decorations.has(Decoration::Uniform)
        })
        .copied()
        .collect::<Vec<_>>();
    let mut eliminated_uniforms = HashSet::new();
    for uniform_id in uniforms {
        let (type_id, arrayness) = get_variable_base_type_and_arrayness(ir_meta, uniform_id);

        // For every uniform, first check if it's a struct with any samplers
        let info = &struct_info[type_id.id as usize];
        if !info.has_sampler {
            continue;
        }

        // For every sampler field, declare a new sampler uniform.
        let path = PathToSampler { uniform_var: uniform_id, fields: vec![] };
        extract_samplers(
            ir_meta,
            &mut extracted_sampler_map,
            struct_info,
            info,
            path,
            arrayness,
            &mut next_sampler_index,
        );

        // In the end, if the uniform had only samplers, eliminate it
        if info.first_sampler_only_field == 0 {
            ir_meta.dead_code_eliminate_variable(uniform_id);
            eliminated_uniforms.insert(uniform_id);
        }
    }

    ir_meta.prune_global_variables(|variable_id, _| !eliminated_uniforms.contains(&variable_id));
    extracted_sampler_map
}

fn extract_samplers(
    ir_meta: &mut IRMeta,
    extracted_sampler_map: &mut HashMap<PathToSampler, TypedId>,
    struct_info: &Vec<StructInfo>,
    info: &StructInfo,
    path: PathToSampler,
    arrayness: Vec<u32>,
    next_sampler_index: &mut u32,
) {
    for field_index in info.first_sampler_field..info.field_info.len() {
        let field_info = &info.field_info[field_index];
        let field_arrayness = [&arrayness[..], &field_info.arrayness[..]].concat();
        let mut field_path = path.clone();
        field_path.fields.push(field_index as u32);

        // If this field is a sampler itself, declare the sampler.  Otherwise, recursively extract
        // samplers from the field.
        if ir_meta.get_type(field_info.base_type_id).is_sampled_image() {
            extract_sampler(
                ir_meta,
                extracted_sampler_map,
                field_path,
                field_info.base_type_id,
                field_arrayness,
                field_info.precision,
                next_sampler_index,
            );
        } else if struct_info[field_info.base_type_id.id as usize].has_sampler {
            extract_samplers(
                ir_meta,
                extracted_sampler_map,
                struct_info,
                &struct_info[field_info.base_type_id.id as usize],
                field_path,
                field_arrayness,
                next_sampler_index,
            );
        }
    }
}

fn extract_sampler(
    ir_meta: &mut IRMeta,
    extracted_sampler_map: &mut HashMap<PathToSampler, TypedId>,
    path: PathToSampler,
    base_type_id: TypeId,
    arrayness: Vec<u32>,
    precision: Precision,
    next_sampler_index: &mut u32,
) {
    let type_id = arrayness
        .iter()
        .rev()
        .fold(base_type_id, |type_id, &size| ir_meta.get_array_type_id(type_id, size));

    // Declare a uniform for this sampler.
    // Note: the name must match kExtractedSamplerNamePrefix.
    let sampler = ir_meta
        .declare_variable(
            Name::new_exact_with_suffix("extractedSampler", *next_sampler_index),
            type_id,
            precision,
            false,
            Decorations::new(vec![Decoration::Uniform]),
            None,
            None,
            VariableScope::Global,
        )
        .1;

    extracted_sampler_map.insert(path, sampler);
    *next_sampler_index += 1;
}

fn access_sampler(state: &mut State, result: TypedRegisterId) -> Vec<traverser::Transform> {
    let mut transforms = vec![];

    // If reached a sampler, replace the access chain up to it (if tracked) with one accessing the
    // replacement uniform.
    let result_type_id = state.ir_meta.get_pointee_type(result.type_id);
    if state.ir_meta.get_type(result_type_id).is_sampled_image()
        && let Some(ref access_chain) = state.access_chains[result.id.id as usize]
    {
        // Look up the extracted sampler uniform, and generate the following access chain and
        // Load:
        //
        //    indexed0 = AccessArrayElement sampler indices[0]
        //    indexed1 = AccessArrayElement indexed0 indices[1]
        //    ...
        //    result = AccessArrayElement ...
        let mut indexee = *state.extracted_sampler_map.get(&access_chain.path).unwrap();
        if let Some((&last_index, leading_indices)) = access_chain.indices.split_last() {
            for &index in leading_indices.iter() {
                indexee = traverser::add_typed_instruction(
                    &mut transforms,
                    instruction::make!(index, state.ir_meta, indexee, index),
                );
            }
            traverser::add_typed_instruction(
                &mut transforms,
                instruction::make_with_result_id!(
                    index,
                    state.ir_meta,
                    result,
                    indexee,
                    last_index
                ),
            );
        } else {
            // If there are no indices, alias the resulting access chain with the uniform.
            //
            //    result = Alias sampler
            traverser::add_typed_instruction(
                &mut transforms,
                instruction::make_with_result_id!(alias, state.ir_meta, result, indexee),
            );
        }
    }

    transforms
}

fn transform_struct_field(
    state: &mut State,
    pointer: TypedId,
    field: u32,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let base_type_id = state.ir_meta.get_pointee_type(pointer.type_id);
    let struct_info = &state.struct_info[base_type_id.id as usize];

    // Don't track access chains that don't lead to samplers
    if !struct_info.has_sampler
        || (field as usize) < struct_info.first_sampler_field
        || !struct_info.field_info[field as usize].has_sampler
    {
        return vec![];
    }

    let mut accesses_sampler_only_field = field as usize >= struct_info.first_sampler_only_field;

    match pointer.id {
        Id::Variable(variable_id) => {
            let decorations = &state.ir_meta.get_variable(variable_id).decorations;
            let is_uniform = decorations.has(Decoration::Uniform)
                && !has_decoration!(decorations, Decoration::Block);
            if is_uniform {
                // Start tracking this access chain, it may lead to a sampler access
                state.access_chains[result.id.id as usize] = Some(AccessChain {
                    path: PathToSampler { uniform_var: variable_id, fields: vec![field] },
                    indices: vec![],
                    accesses_sampler_only_field,
                });
            }
        }
        Id::Register(register_id) => {
            // If the access chain is tracked for this pointer, continue tracking it.
            if let Some(ref pointer_access_chain) = state.access_chains[register_id.id as usize] {
                let mut access_chain = pointer_access_chain.clone();
                access_chain.path.fields.push(field);
                accesses_sampler_only_field =
                    accesses_sampler_only_field || access_chain.accesses_sampler_only_field;
                access_chain.accesses_sampler_only_field = accesses_sampler_only_field;

                state.access_chains[result.id.id as usize] = Some(access_chain);
            }
        }
        Id::Constant(..) => panic!("Internal error: Unexpected AccessStructField on a constant"),
    };

    // If reached a sampler, replace the access chain up to it (if tracked) with one accessing the
    // replacement uniform.
    let replacement = access_sampler(state, result);
    if !replacement.is_empty() {
        replacement
    } else if accesses_sampler_only_field {
        // If the access is to a field that's removed, remove this instruction
        vec![traverser::Transform::Remove]
    } else {
        vec![]
    }
}

fn transform_array_element(
    state: &mut State,
    pointer: TypedId,
    index: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let accesses_sampler_only_field = match pointer.id {
        Id::Variable(variable_id) => {
            let base_type_id = get_variable_base_type(state.ir_meta, variable_id);
            let struct_info = &state.struct_info[base_type_id.id as usize];
            if !struct_info.has_sampler {
                return vec![];
            }

            let decorations = &state.ir_meta.get_variable(variable_id).decorations;
            let is_uniform = decorations.has(Decoration::Uniform)
                && !has_decoration!(decorations, Decoration::Block);
            if is_uniform {
                // Start tracking this access chain, it may lead to a sampler access
                let accesses_sampler_only_field = struct_info.first_sampler_only_field == 0;
                state.access_chains[result.id.id as usize] = Some(AccessChain {
                    path: PathToSampler { uniform_var: variable_id, fields: vec![] },
                    indices: vec![index],
                    accesses_sampler_only_field,
                });
                accesses_sampler_only_field
            } else {
                false
            }
        }
        Id::Register(register_id) => {
            // If the access chain is tracked for this pointer, continue tracking it.
            if let Some(ref pointer_access_chain) = state.access_chains[register_id.id as usize] {
                let mut access_chain = pointer_access_chain.clone();
                access_chain.indices.push(index);
                let accesses_sampler_only_field = access_chain.accesses_sampler_only_field;

                state.access_chains[result.id.id as usize] = Some(access_chain);
                accesses_sampler_only_field
            } else {
                false
            }
        }
        Id::Constant(..) => panic!("Internal error: Unexpected AccessArrayElement on a constant"),
    };

    // If reached a sampler, replace the access chain up to it (if tracked) with one accessing the
    // replacement uniform.
    let replacement = access_sampler(state, result);
    if !replacement.is_empty() {
        replacement
    } else if accesses_sampler_only_field {
        // If the access is to a field that's removed, remove this instruction
        vec![traverser::Transform::Remove]
    } else {
        vec![]
    }
}
