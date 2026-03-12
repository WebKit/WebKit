// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Turn `inout` variables that are never read from into |out| before collecting variables and
// before PLS uses them.
//
// An `inout` variable must remain as `inout` under the following conditions:
//
// - If not all channels are written to (and the unwritten channels should be preserved)
// - If the shader reads from the variable before fully overwriting it
// - If the shader has `discard`
//
// Detecting the above conditions perfectly can be very tricky.  To simplify this transformation
// and detect the most common cases, the following assumptions are made:
//
// - Any writes other than directly under `main()`'s body (not nested) do not count, because they
//   may be conditionally done.
// - Any reads anywhere will make the variable stay as `inout`.
//
use crate::ir::*;
use crate::*;

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    // Whether a read operation has been performed on the variable
    is_read: Vec<bool>,
    // Which channels of the variable have been written to in main()
    written_channels: Vec<u8>,
    is_in_main: bool,
    shader_has_discard: bool,
}

pub fn run(ir: &mut IR) {
    let is_read = vec![false; ir.meta.all_variables().len()];
    let written_channels = vec![0u8; ir.meta.all_variables().len()];

    let mut state = State {
        ir_meta: &mut ir.meta,
        is_read,
        written_channels,
        is_in_main: false,
        shader_has_discard: false,
    };

    traverser::visitor::for_each_function(
        &mut state,
        &mut ir.function_entries,
        |state, function_id| {
            state.is_in_main =
                state.ir_meta.get_main_function_id().map(|id| id == function_id).unwrap();
        },
        |state, block, _, depth| {
            visit_block(state, block, depth);
            traverser::visitor::VISIT_SUB_BLOCKS
        },
        |_, _| {},
    );

    // Turn `inout` fragment outputs to `out` if they are never read from and completely
    // overwritten.
    if !state.shader_has_discard {
        for variable_id in state.ir_meta.all_global_variables().clone() {
            // Must not be read from
            if state.is_read[variable_id.id as usize] {
                continue;
            }

            // Must be an `inout` variable
            let variable = state.ir_meta.get_variable(variable_id);
            if !variable.decorations.has(Decoration::InputOutput) {
                continue;
            }

            // Must have written to all channels
            let type_info = state.ir_meta.get_type(variable.type_id);
            debug_assert!(type_info.is_pointer());
            let type_info = state.ir_meta.get_type(type_info.get_element_type_id().unwrap());
            let all_channels = if type_info.is_scalar() {
                1u8
            } else if let Some(vector_size) = type_info.get_vector_size() {
                ((1 << vector_size) - 1) as u8
            } else {
                continue;
            };
            if state.written_channels[variable_id.id as usize] & all_channels == all_channels {
                // Turn `inout` into `out`
                for decoration in
                    state.ir_meta.get_variable_mut(variable_id).decorations.decorations.iter_mut()
                {
                    if *decoration == Decoration::InputOutput {
                        *decoration = Decoration::Output;
                        break;
                    }
                }
            }
        }
    }
}

fn read_pointer(ir_meta: &IRMeta, is_read: &mut Vec<bool>, pointer: TypedId) {
    let mut pointer = pointer;
    while let Id::Register(register_id) = pointer.id {
        match ir_meta.get_instruction(register_id).op {
            OpCode::Alias(id)
            | OpCode::AccessVectorComponent(id, _)
            | OpCode::AccessVectorComponentMulti(id, _)
            | OpCode::AccessVectorComponentDynamic(id, _)
            | OpCode::AccessMatrixColumn(id, _)
            | OpCode::AccessStructField(id, _)
            | OpCode::AccessArrayElement(id, _) => {
                pointer = id;
            }
            _ => {
                panic!("Internal error: Unexpected value-producing instruction");
            }
        }
    }

    // Mark the whole variable as read for simplicity.
    if let Id::Variable(variable_id) = pointer.id {
        is_read[variable_id.id as usize] = true;
    } else {
        panic!("Internal error: Unexpected constant where a pointer is expected");
    }
}

fn write_pointer(
    ir_meta: &IRMeta,
    written_channels: &mut Vec<u8>,
    pointer: TypedId,
    depth: usize,
    is_in_main: bool,
) {
    let mut pointer = pointer;

    // Don't count writes that may never execute.
    if depth > 0 || !is_in_main {
        return;
    }

    let mut channels = 0u8;
    while let Id::Register(register_id) = pointer.id {
        match ir_meta.get_instruction(register_id).op {
            OpCode::Alias(id) => {
                pointer = id;
            }
            OpCode::AccessVectorComponent(id, index) => {
                pointer = id;
                // Swizzle of swizzle is never generated due to folding.
                debug_assert!(channels == 0);
                channels |= (1 << index) as u8;
            }
            OpCode::AccessVectorComponentMulti(id, ref indices) => {
                pointer = id;
                // Swizzle of swizzle is never generated due to folding.
                debug_assert!(channels == 0);
                for index in indices {
                    channels |= (1 << index) as u8;
                }
            }
            OpCode::AccessVectorComponentDynamic(..)
            | OpCode::AccessMatrixColumn(..)
            | OpCode::AccessStructField(..)
            | OpCode::AccessArrayElement(..) => {
                // `inout` variables cannot be structs or matrices.  For simplicity, we also ignore
                // arrays and keep them as `inout`.  When the vector component is not a constant,
                // we can't track which channels are written to.  In all the above cases, assume
                // the output is not written to.
                return;
            }
            _ => {
                panic!("Internal error: Unexpected value-producing instruction");
            }
        }
    }

    // Mark the channels that have been written to.
    if let Id::Variable(variable_id) = pointer.id {
        // If channels is zero, it means that swizzle was not used, so the whole variable
        // is being written to.
        written_channels[variable_id.id as usize] |= if channels == 0 { 0xFu8 } else { channels };
    } else {
        panic!("Internal error: Unexpected constant where a pointer is expected");
    }
}

fn visit_block(state: &mut State, block: &Block, depth: usize) {
    for instruction in block.instructions.iter() {
        let (opcode, _) = instruction.get_op_and_result(state.ir_meta);

        match opcode {
            OpCode::Discard => {
                state.shader_has_discard = true;
            },
            // Read accesses
            &OpCode::Load(pointer) |
            &OpCode::Unary(UnaryOpCode::PrefixIncrement, pointer) |
            &OpCode::Unary(UnaryOpCode::PrefixDecrement, pointer) |
            &OpCode::Unary(UnaryOpCode::PostfixIncrement, pointer) |
            &OpCode::Unary(UnaryOpCode::PostfixDecrement, pointer) |
            // While the atomic*() functions don't make sense for color outputs, there's nothing
            // preventing a shader from using them.
            &OpCode::Binary(BinaryOpCode::AtomicAdd, pointer, _) |
            &OpCode::Binary(BinaryOpCode::AtomicMin, pointer, _) |
            &OpCode::Binary(BinaryOpCode::AtomicMax, pointer, _) |
            &OpCode::Binary(BinaryOpCode::AtomicAnd, pointer, _) |
            &OpCode::Binary(BinaryOpCode::AtomicOr, pointer, _) |
            &OpCode::Binary(BinaryOpCode::AtomicXor, pointer, _) |
            &OpCode::Binary(BinaryOpCode::AtomicExchange, pointer, _) => {
                read_pointer(state.ir_meta, &mut state.is_read, pointer);
            }

            // Write accesses
            &OpCode::Store(pointer, _) |
            &OpCode::Binary(BinaryOpCode::Modf, _, pointer) |
            &OpCode::Binary(BinaryOpCode::Frexp, _, pointer) => {
                write_pointer(state.ir_meta, &mut state.written_channels, pointer, depth, state.is_in_main);
            },
            OpCode::BuiltIn(BuiltInOpCode::UaddCarry, args) |
            OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, args) => {
                write_pointer(state.ir_meta, &mut state.written_channels, args[2], depth, state.is_in_main);
            }
            OpCode::BuiltIn(BuiltInOpCode::UmulExtended, args) |
            OpCode::BuiltIn(BuiltInOpCode::ImulExtended, args) => {
                write_pointer(state.ir_meta, &mut state.written_channels, args[3], depth, state.is_in_main);
                write_pointer(state.ir_meta, &mut state.written_channels, args[2], depth, state.is_in_main);
            },

            // Calls could read or write from variables depending on the parameter direction.
            &OpCode::Call(function_id, ref args) => {
                let param_directions = state.ir_meta.get_function(function_id).params.iter().map(|param| param.direction);
                args.iter().zip(param_directions).for_each(|(&arg, direction)| {
                    // `out` and `inout` parameters are always pointers.  Treat `inout` parameters
                    // as being read.
                    match direction {
                        FunctionParamDirection::Input => {
                            if state.ir_meta.get_type(arg.type_id).is_pointer() {
                                read_pointer(state.ir_meta, &mut state.is_read, arg);
                            }
                        },
                        FunctionParamDirection::InputOutput => {
                            read_pointer(state.ir_meta, &mut state.is_read, arg);
                        },
                        FunctionParamDirection::Output => {
                            write_pointer(state.ir_meta, &mut state.written_channels, arg, depth, state.is_in_main);
                        },
                    };
                });
            },
            _ => {},
        };
    }
}
