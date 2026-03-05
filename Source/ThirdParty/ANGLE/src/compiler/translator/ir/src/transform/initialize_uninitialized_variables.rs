// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Initialize uninitialized variables based on a set of flags:
//
// - If required, all local, global and function parameter variables that were not initialized
//   during parse are zero-initialized.
// - If required, all shader output variables, or just fragment shader output variables are
//   zero-initialized.
// - If required, gl_Position is zero-initialized
//
// Either way, zero initialization may either be done via an initializer or assignment
// instructions.  If the variable being initialized via instructions is a global variable, it is
// initialized at the beginning of main, otherwise it is initialized at the beginning of the block
// that defines it.  Based on flags, a loop may or may not be used to initialize an array.
use crate::ir::*;
use crate::*;

pub struct Options {
    pub loops_allowed_when_initializing_variables: bool,
    pub initializer_allowed_on_non_constant_global_variables: bool,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
}

pub fn run(ir: &mut IR, options: &Options) {
    let mut state = State { ir_meta: &mut ir.meta };

    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, function_id, entry| {
            // Initialize function parameters that need initialization.
            {
                let function = state.ir_meta.get_function(function_id);
                let to_initialize = function
                    .params
                    .iter()
                    .filter_map(|param| {
                        state
                            .ir_meta
                            .variable_needs_zero_initialization(param.variable_id)
                            .then_some(param.variable_id)
                    })
                    .collect::<Vec<_>>();
                make_init_block(state.ir_meta, to_initialize, entry, options);
            }

            traverser::transformer::for_each_block(
                state,
                entry,
                &|state, block| {
                    let to_initialize = block
                        .variables
                        .iter()
                        .filter(|&variable_id| {
                            state.ir_meta.variable_needs_zero_initialization(*variable_id)
                        })
                        .copied()
                        .collect::<Vec<_>>();
                    make_init_block(state.ir_meta, to_initialize, block, options);
                    block
                },
                &|_, block| block,
            )
        },
    );

    // Do the same for global variables
    {
        let to_initialize = state
            .ir_meta
            .all_global_variables()
            .iter()
            .filter(|&variable_id| state.ir_meta.variable_needs_zero_initialization(*variable_id))
            .copied()
            .collect::<Vec<_>>();
        let main_id = state.ir_meta.get_main_function_id().unwrap();
        let main_entry = ir.function_entries[main_id.id as usize].as_mut().unwrap();
        make_init_block(state.ir_meta, to_initialize, main_entry, options);
    }
}

fn is_initializer_allowed(
    ir_meta: &IRMeta,
    id: VariableId,
    decorations: &Decorations,
    built_in: Option<BuiltIn>,
    options: &Options,
) -> bool {
    // If the initializer value is a constant, it can be set as the initializer.  However, that
    // is disallowed in the following case:
    //
    // * initializer_allowed_on_non_constant_global_variables is false
    // * It's the global scope
    // * The variable is not `const`.
    //
    // Additionally, function parameters cannot have an initializer, neither can built-ins and
    // interface variables.
    let variable = ir_meta.get_variable(id);
    variable.scope != VariableScope::FunctionParam
        && built_in.is_none()
        && decorations.decorations.is_empty()
        && (variable.scope == VariableScope::Local
            || options.initializer_allowed_on_non_constant_global_variables
            || variable.is_const)
}

fn initialize_with_zeros<'block>(
    ir_meta: &mut IRMeta,
    mut block: &'block mut Block,
    id: TypedId,
    allow_loops: bool,
) -> &'block mut Block {
    let type_info = ir_meta.get_type(id.type_id);
    debug_assert!(type_info.is_pointer());
    let type_id = type_info.get_element_type_id().unwrap();
    let type_info = ir_meta.get_type(type_id);

    match *type_info {
        Type::Struct(_, ref fields, _) => {
            // For structs, initialize field by field.
            for index in 0..fields.len() {
                // selected_field = AccessStructField id index
                let selected_field = block.add_typed_instruction(instruction::struct_field(
                    ir_meta,
                    id,
                    index as u32,
                ));
                // Recursively set the field to zeros.
                block = initialize_with_zeros(ir_meta, block, selected_field, allow_loops);
            }
        }
        Type::Array(element_id, count) => {
            // For arrays, initialize element by element, either with a loop or unrolled.
            let element_type_info = ir_meta.get_type(element_id);
            let is_small_array = count <= 1
                || (!element_type_info.is_struct() && !element_type_info.is_array() && count <= 3);
            let use_loop = allow_loops && !is_small_array;
            if use_loop {
                // Note: `uint` would be a better loop index type, but ESSL 100 doesn't support
                // that.
                let count_constant =
                    TypedId::from_constant_id(ir_meta.get_constant_int(count as i32), TYPE_ID_INT);

                // Loop variable, int index = 0:
                let loop_index_id = ir_meta.declare_variable(
                    Name::new_temp(""),
                    TYPE_ID_INT,
                    Precision::High,
                    Decorations::new_none(),
                    None,
                    Some(CONSTANT_ID_INT_ZERO),
                    VariableScope::Local,
                );
                block.add_variable_declaration(loop_index_id);
                let loop_index_id = TypedId::from_variable_id(ir_meta, loop_index_id);

                // Add the loop condition, index < count:
                {
                    let mut condition_block = Block::new();
                    // loaded_index = Load loop_index_id
                    let loaded_index = condition_block
                        .add_typed_instruction(instruction::load(ir_meta, loop_index_id));
                    // compare_with_count = LessThan loop_index_id count
                    let compare_with_count = condition_block.add_typed_instruction(
                        instruction::less_than(ir_meta, loaded_index, count_constant),
                    );
                    condition_block.terminate(OpCode::LoopIf(compare_with_count));
                    block.set_loop_condition_block(condition_block);
                }

                // Add the loop continue block, ++index:
                {
                    let mut continue_block = Block::new();
                    continue_block.add_typed_instruction(instruction::prefix_increment(
                        ir_meta,
                        loop_index_id,
                    ));

                    continue_block.terminate(OpCode::Continue);
                    block.set_loop_continue_block(continue_block);
                }

                // Add the loop body, recursively initialize id[index]
                {
                    let mut body_block = Block::new();
                    // loaded_index = Load loop_index_id
                    let loaded_index =
                        body_block.add_typed_instruction(instruction::load(ir_meta, loop_index_id));
                    // selected_element = AccessArrayElement id index
                    let selected_element = body_block.add_typed_instruction(instruction::index(
                        ir_meta,
                        id,
                        loaded_index,
                    ));
                    // Recursively set the element to zeros.
                    let body_last_block = initialize_with_zeros(
                        ir_meta,
                        &mut body_block,
                        selected_element,
                        allow_loops,
                    );

                    body_last_block.terminate(OpCode::Continue);
                    block.set_loop_body_block(body_block);
                }

                let next_block = Block::new();
                block.terminate(OpCode::Loop);
                block.set_merge_block(next_block);
                block = block.get_merge_chain_last_block_mut();
            } else {
                // Note that it is important to have the array init in the right order to
                // workaround a driver bug per http://crbug.com/40514481.
                for index in 0..count {
                    let index_constant = TypedId::from_constant_id(
                        ir_meta.get_constant_int(index as i32),
                        TYPE_ID_INT,
                    );
                    // selected_element = AccessArrayElement id index
                    let selected_element = block.add_typed_instruction(instruction::index(
                        ir_meta,
                        id,
                        index_constant,
                    ));
                    // Recursively set the element to zeros.
                    block = initialize_with_zeros(ir_meta, block, selected_element, allow_loops);
                }
            }
        }
        _ => {
            // For scalars, vectors and matrices, use a zero constant which is not very big.
            let null_value = TypedId::from_constant_id(ir_meta.get_constant_null(type_id), type_id);
            block.add_void_instruction(OpCode::Store(id, null_value));
        }
    }

    block
}

fn make_init_block(
    ir_meta: &mut IRMeta,
    to_initialize: Vec<VariableId>,
    target_block: &mut Block,
    options: &Options,
) {
    if to_initialize.is_empty() {
        return;
    }

    // Create a block to initialize the variables
    let mut init_block = Block::new();
    let mut current_block = &mut init_block;
    let mut any_code_generated = false;

    for &id in to_initialize.iter() {
        let variable = ir_meta.get_variable(id);
        debug_assert!(variable.initializer.is_none());

        let type_info = ir_meta.get_type(variable.type_id);
        debug_assert!(type_info.is_pointer());
        let type_id = type_info.get_element_type_id().unwrap();
        let type_info = ir_meta.get_type(type_id);

        debug_assert!(
            !type_info.is_image()
                && !type_info.is_unsized_array()
                && !variable.decorations.has(Decoration::Input)
                && !variable.decorations.has(Decoration::InputOutput)
                && !variable.decorations.has(Decoration::Uniform)
                && !variable.decorations.has(Decoration::Buffer)
                && !variable.decorations.has(Decoration::Shared)
        );

        // If using an initializer is allowed and the type is simple, create a zero initializer and
        // set it as the initializer for the variable.
        let is_initializer_allowed =
            is_initializer_allowed(ir_meta, id, &variable.decorations, variable.built_in, options);
        let is_fragment_output_array = matches!(variable.built_in, Some(BuiltIn::FragData))
            || (ir_meta.get_shader_type() == ShaderType::Fragment
                && variable.decorations.has(Decoration::Output));

        // TODO(http://anglebug.com/349994211): Eventually it's best for SPIR-V if
        // initialize_with_value is always used because it can use OpConstantNull as the
        // initializer.  This can be done when SPIR-V generation is done from IR, at which point
        // it's likely best not to initialize variables here so a possibly giant constant is not
        // generated in the IR only to be replaced by OpConstantNull, and zero-init variables
        // directly while generating SPIR-V.
        if is_initializer_allowed
            && (type_info.is_scalar() || type_info.is_vector() || type_info.is_matrix())
        {
            let null_value = ir_meta.get_constant_null(type_id);
            ir_meta.set_variable_initializer(id, null_value);
        } else {
            let variable = TypedId::from_variable_id(ir_meta, id);
            // For gl_FragData, the array elements are assigned one by one to keep the AST
            // compatible with ESSL 1.00 which doesn't have array assignment.
            current_block = initialize_with_zeros(
                ir_meta,
                current_block,
                variable,
                options.loops_allowed_when_initializing_variables && !is_fragment_output_array,
            );
            ir_meta.on_variable_zero_initialization_done(id);
            any_code_generated = true;
        }
    }

    // Make sure the init block is prepended to the block that is expected to initialize the
    // variables.
    if any_code_generated {
        // Because the init_block may reference variables that are declared in target_block, the
        // declaration of those variables need to be moved to init_block.  This is only needed when
        // initialization function-local variables.
        let (mut to_move, to_keep): (Vec<_>, Vec<_>) = target_block
            .variables
            .iter()
            .partition(|variable_id| to_initialize.contains(variable_id));
        target_block.variables = to_keep;
        init_block.variables.append(&mut to_move);

        target_block.prepend_code(init_block);
    }
}
