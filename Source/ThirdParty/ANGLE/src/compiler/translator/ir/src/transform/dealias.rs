// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Remove Alias instructions from the IR.  Instructions in the form `%new = Alias %original`
// are removed and `%original.id` is made to replace every instance of `%new.id`.  The types
// are identical and untouched, and the precision of `%new` is retained.
use crate::ir::*;
use crate::*;

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    alias_map: HashMap<RegisterId, Id>,
}

pub fn run(ir: &mut IR) {
    let mut state = State { ir_meta: &mut ir.meta, alias_map: HashMap::new() };

    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        |_, _| {},
        &|state, entry| {
            traverser::transformer::for_each_block(
                state,
                entry,
                &|state, block| dealias_registers(state, block),
                &|_, block| block,
            )
        },
    );
}

fn dealias_registers<'block>(state: &mut State, block: &'block mut Block) -> &'block mut Block {
    let instructions = std::mem::replace(&mut block.instructions, vec![]);
    let mut transformed = Vec::with_capacity(instructions.len());

    for mut instruction in instructions.into_iter() {
        let (opcode, result) = match &mut instruction {
            BlockInstruction::Void(op) => (op, None),
            &mut BlockInstruction::Register(id) => {
                (&mut state.ir_meta.get_instruction_mut(id).op, Some(id))
            }
        };

        match opcode {
            OpCode::MergeInput
            | OpCode::Discard
            | OpCode::Break
            | OpCode::Continue
            | OpCode::Passthrough
            | OpCode::NextBlock
            | OpCode::Loop
            | OpCode::DoLoop
            | OpCode::Return(None)
            | OpCode::Merge(None) => (),
            OpCode::Call(_, params)
            | OpCode::ConstructVectorFromMultiple(params)
            | OpCode::ConstructMatrixFromMultiple(params)
            | OpCode::ConstructStruct(params)
            | OpCode::ConstructArray(params)
            | OpCode::BuiltIn(_, params) => {
                for param in params {
                    param.dealias(&state.alias_map);
                }
            }
            OpCode::Return(Some(id))
            | OpCode::Merge(Some(id))
            | OpCode::If(id)
            | OpCode::LoopIf(id)
            | OpCode::Switch(id, _)
            | OpCode::ExtractVectorComponent(id, _)
            | OpCode::ExtractVectorComponentMulti(id, _)
            | OpCode::ExtractStructField(id, _)
            | OpCode::ConstructScalarFromScalar(id)
            | OpCode::ConstructVectorFromScalar(id)
            | OpCode::ConstructMatrixFromScalar(id)
            | OpCode::ConstructMatrixFromMatrix(id)
            | OpCode::AccessVectorComponent(id, _)
            | OpCode::AccessVectorComponentMulti(id, _)
            | OpCode::AccessStructField(id, _)
            | OpCode::Load(id)
            | OpCode::Unary(_, id) => id.dealias(&state.alias_map),
            OpCode::ExtractVectorComponentDynamic(lhs, rhs)
            | OpCode::ExtractMatrixColumn(lhs, rhs)
            | OpCode::ExtractArrayElement(lhs, rhs)
            | OpCode::AccessVectorComponentDynamic(lhs, rhs)
            | OpCode::AccessMatrixColumn(lhs, rhs)
            | OpCode::AccessArrayElement(lhs, rhs)
            | OpCode::Store(lhs, rhs)
            | OpCode::Binary(_, lhs, rhs) => {
                lhs.dealias(&state.alias_map);
                rhs.dealias(&state.alias_map);
            }
            OpCode::Texture(texture_op, sampler, coord) => {
                sampler.dealias(&state.alias_map);
                coord.dealias(&state.alias_map);
                match texture_op {
                    TextureOpCode::Implicit { is_proj: _, offset }
                    | TextureOpCode::Gather { offset } => {
                        offset.as_mut().map(|id| id.dealias(&state.alias_map));
                    }
                    TextureOpCode::Compare { compare } => {
                        compare.dealias(&state.alias_map);
                    }
                    TextureOpCode::Lod { is_proj: _, lod, offset } => {
                        lod.dealias(&state.alias_map);
                        offset.as_mut().map(|id| id.dealias(&state.alias_map));
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        compare.dealias(&state.alias_map);
                        lod.dealias(&state.alias_map);
                    }
                    TextureOpCode::Bias { is_proj: _, bias, offset } => {
                        bias.dealias(&state.alias_map);
                        offset.as_mut().map(|id| id.dealias(&state.alias_map));
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        compare.dealias(&state.alias_map);
                        bias.dealias(&state.alias_map);
                    }
                    TextureOpCode::Grad { is_proj: _, dx, dy, offset } => {
                        dx.dealias(&state.alias_map);
                        dy.dealias(&state.alias_map);
                        offset.as_mut().map(|id| id.dealias(&state.alias_map));
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        component.dealias(&state.alias_map);
                        offset.as_mut().map(|id| id.dealias(&state.alias_map));
                    }
                    TextureOpCode::GatherRef { refz, offset } => {
                        refz.dealias(&state.alias_map);
                        offset.as_mut().map(|id| id.dealias(&state.alias_map));
                    }
                }
            }
            OpCode::Alias(_) => {
                let register_id = result.unwrap();
                state.alias_map.insert(register_id, state.ir_meta.get_aliased_id(register_id).id);
                continue;
            }
        };

        transformed.push(instruction);
    }

    block.instructions = transformed;

    block
}
