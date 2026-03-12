// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper to turn the IR into a GLSL AST.  For the GLSL generator, this is used to generate the
// shader itself.  While the translator is not yet fully converted to use the IR, this is used to
// drop back to the legacy AST.

use crate::ir::*;
use crate::*;

pub trait Target {
    type BlockResult;

    fn begin(&mut self);
    fn end(&mut self) -> Self::BlockResult;

    // Upfront, create objects that would later represent types, constants, variables and
    // functions.  Later, the generator references these objects by ID and the implementation
    // can look these objects up based on that.
    fn new_type(&mut self, ir_meta: &IRMeta, id: TypeId, type_info: &Type);
    fn new_constant(&mut self, ir_meta: &IRMeta, id: ConstantId, constant: &Constant);
    fn new_variable(&mut self, ir_meta: &IRMeta, id: VariableId, variable: &Variable);
    fn new_function(&mut self, ir_meta: &IRMeta, id: FunctionId, function: &Function);

    // Set up everything that needs to be set up in the global scope.  That includes global
    // variables, geometry/tessellation info, etc.
    fn global_scope(&mut self, ir_meta: &IRMeta);

    fn begin_block(&mut self, ir_meta: &IRMeta, variables: &Vec<VariableId>) -> Self::BlockResult;
    fn merge_blocks(&mut self, blocks: Vec<Self::BlockResult>) -> Self::BlockResult;

    // Instructions
    fn swizzle_single(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        id: TypedId,
        index: u32,
    );
    fn swizzle_multi(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        id: TypedId,
        indices: &Vec<u32>,
    );
    fn index(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        id: TypedId,
        index: TypedId,
    );
    fn select_field(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        id: TypedId,
        index: u32,
        field: &Field,
    );
    fn construct_single(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        type_id: TypeId,
        id: TypedId,
    );
    fn construct_multi(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        type_id: TypeId,
        ids: &Vec<TypedId>,
    );
    fn load(&mut self, block: &mut Self::BlockResult, result: RegisterId, pointer: TypedId);
    fn store(&mut self, block: &mut Self::BlockResult, pointer: TypedId, value: TypedId);
    fn call(
        &mut self,
        block: &mut Self::BlockResult,
        result: Option<RegisterId>,
        function_id: FunctionId,
        params: &Vec<TypedId>,
    );
    fn unary(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        unary_op: UnaryOpCode,
        id: TypedId,
    );
    fn binary(
        &mut self,
        block: &mut Self::BlockResult,
        result: RegisterId,
        binary_op: BinaryOpCode,
        lhs: TypedId,
        rhs: TypedId,
    );
    fn built_in(
        &mut self,
        block: &mut Self::BlockResult,
        result: Option<RegisterId>,
        built_in_op: BuiltInOpCode,
        args: &Vec<TypedId>,
    );
    fn texture(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        offset: Option<TypedId>,
    );
    fn texture_compare(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        compare: TypedId,
    );
    fn texture_lod(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        lod: TypedId,
        offset: Option<TypedId>,
    );
    fn texture_compare_lod(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        compare: TypedId,
        lod: TypedId,
    );
    fn texture_bias(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        bias: TypedId,
        offset: Option<TypedId>,
    );
    fn texture_compare_bias(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        compare: TypedId,
        bias: TypedId,
    );
    fn texture_grad(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        dx: TypedId,
        dy: TypedId,
        offset: Option<TypedId>,
    );
    fn texture_gather(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        offset: Option<TypedId>,
    );
    fn texture_gather_component(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        component: TypedId,
        offset: Option<TypedId>,
    );
    fn texture_gather_ref(
        &mut self,
        ir_meta: &IRMeta,
        block: &mut Self::BlockResult,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        refz: TypedId,
        offset: Option<TypedId>,
    );

    // Control flow
    fn branch_discard(&mut self, block: &mut Self::BlockResult);
    fn branch_return(&mut self, block: &mut Self::BlockResult, value: Option<TypedId>);
    fn branch_break(&mut self, block: &mut Self::BlockResult);
    fn branch_continue(&mut self, block: &mut Self::BlockResult);
    fn branch_if(
        &mut self,
        block: &mut Self::BlockResult,
        condition: TypedId,
        true_block: Option<Self::BlockResult>,
        false_block: Option<Self::BlockResult>,
    );
    fn branch_loop(
        &mut self,
        block: &mut Self::BlockResult,
        loop_condition_block: Option<Self::BlockResult>,
        body_block: Option<Self::BlockResult>,
    );
    fn branch_do_loop(
        &mut self,
        block: &mut Self::BlockResult,
        body_block: Option<Self::BlockResult>,
    );
    fn branch_loop_if(&mut self, block: &mut Self::BlockResult, condition: TypedId);
    fn branch_switch(
        &mut self,
        block: &mut Self::BlockResult,
        value: TypedId,
        case_ids: &Vec<Option<ConstantId>>,
        case_blocks: Vec<Self::BlockResult>,
    );

    // Take the current AST and place it as the body of the given function.
    fn end_function(&mut self, block: Self::BlockResult, id: FunctionId);
}

pub struct Generator {
    ir: IR,
}

impl Generator {
    pub fn new(ir: IR) -> Generator {
        Generator { ir }
    }

    // Note: call transform::dealias::run() beforehand, as well as transform::astify::run().
    pub fn generate<T: Target>(&mut self, target: &mut T) -> T::BlockResult {
        // Declare the types, variables and functions up-front so they can be referred to by ids
        // when generating the AST itself.
        self.create_types(target);
        self.create_constants(target);
        self.create_variables(target);
        self.create_functions(target);

        self.generate_ast(target)
    }

    fn create_types<T: Target>(&self, target: &mut T) {
        // TODO(http://anglebug.com/349994211): Don't declare types that have been eliminated (such
        // as unused structs)
        self.ir.meta.all_types().iter().enumerate().for_each(|(id, type_info)| {
            target.new_type(&self.ir.meta, TypeId { id: id as u32 }, type_info)
        });
    }

    fn create_constants<T: Target>(&self, target: &mut T) {
        // TODO(http://anglebug.com/349994211): Don't declare constants that have been eliminated
        // (such as constants created out of unused structs).  See for example:
        // GLSLTest.StructUsedWithoutVariable/*
        self.ir.meta.all_constants().iter().enumerate().for_each(|(id, constant)| {
            target.new_constant(&self.ir.meta, ConstantId { id: id as u32 }, constant)
        });
    }

    fn create_variables<T: Target>(&self, target: &mut T) {
        self.ir.meta.all_variables().iter().enumerate().for_each(|(id, variable)| {
            target.new_variable(&self.ir.meta, VariableId { id: id as u32 }, variable)
        });
    }

    fn create_functions<T: Target>(&self, target: &mut T) {
        self.ir.meta.all_functions().iter().enumerate().for_each(|(id, function)| {
            target.new_function(&self.ir.meta, FunctionId { id: id as u32 }, function)
        });
    }

    fn generate_ast<T: Target>(&self, target: &mut T) -> T::BlockResult {
        target.begin();

        // Prepare the global scope
        target.global_scope(&self.ir.meta);

        // Visit the functions in DAG-sorted order, so that forward declarations are
        // unnecessary.
        let function_decl_order =
            util::calculate_function_decl_order(&self.ir.meta, &self.ir.function_entries);

        for function_id in function_decl_order {
            let entry = &self.ir.function_entries[function_id.id as usize].as_ref().unwrap();

            let result_body = self.generate_block(entry, target);
            target.end_function(result_body, function_id);
        }

        target.end()
    }

    fn generate_block<T: Target>(&self, block: &Block, target: &mut T) -> T::BlockResult {
        traverser::visitor::visit_block_instructions(
            &mut (self, target),
            block,
            &|(generator, target), block: &Block| {
                // transform::astify::run() should have gotten rid of merge block inputs.
                debug_assert!(block.input.is_none());
                target.begin_block(&generator.ir.meta, &block.variables)
            },
            &|(generator, target), block_result, instructions| {
                generator.generate_instructions(block_result, instructions, *target);
            },
            &|(generator, target),
              block_result,
              branch_opcode,
              loop_condition_block_result,
              block1_result,
              block2_result,
              case_block_results| {
                generator.generate_branch_instruction(
                    block_result,
                    branch_opcode,
                    loop_condition_block_result,
                    block1_result,
                    block2_result,
                    case_block_results,
                    *target,
                );
            },
            &|(_, target), block_result_chain| target.merge_blocks(block_result_chain),
        )
    }

    fn generate_instructions<T: Target>(
        &self,
        block_result: &mut T::BlockResult,
        instructions: &[BlockInstruction],
        target: &mut T,
    ) {
        // Generate nodes for all instructions except the terminating branch instruction.
        instructions.iter().for_each(|instruction| {
            let (op, result) = instruction.get_op_and_result(&self.ir.meta);
            match op {
                &OpCode::ExtractVectorComponent(id, index)
                | &OpCode::AccessVectorComponent(id, index) => {
                    target.swizzle_single(block_result, result.unwrap().id, id, index)
                }
                &OpCode::ExtractVectorComponentMulti(id, ref indices)
                | &OpCode::AccessVectorComponentMulti(id, ref indices) => {
                    target.swizzle_multi(block_result, result.unwrap().id, id, indices)
                }
                &OpCode::ExtractVectorComponentDynamic(id, index)
                | &OpCode::AccessVectorComponentDynamic(id, index)
                | &OpCode::ExtractMatrixColumn(id, index)
                | &OpCode::AccessMatrixColumn(id, index)
                | &OpCode::ExtractArrayElement(id, index)
                | &OpCode::AccessArrayElement(id, index) => {
                    target.index(block_result, result.unwrap().id, id, index)
                }
                &OpCode::ExtractStructField(id, index) => target.select_field(
                    block_result,
                    result.unwrap().id,
                    id,
                    index,
                    &self.ir.meta.get_type(id.type_id).get_struct_field(index),
                ),
                &OpCode::AccessStructField(id, index) => {
                    let struct_type_id =
                        self.ir.meta.get_type(id.type_id).get_element_type_id().unwrap();
                    target.select_field(
                        block_result,
                        result.unwrap().id,
                        id,
                        index,
                        &self.ir.meta.get_type(struct_type_id).get_struct_field(index),
                    )
                }

                &OpCode::ConstructScalarFromScalar(id)
                | &OpCode::ConstructVectorFromScalar(id)
                | &OpCode::ConstructMatrixFromScalar(id)
                | &OpCode::ConstructMatrixFromMatrix(id) => {
                    let result = result.unwrap();
                    target.construct_single(block_result, result.id, result.type_id, id);
                }
                &OpCode::ConstructVectorFromMultiple(ref ids)
                | &OpCode::ConstructMatrixFromMultiple(ref ids)
                | &OpCode::ConstructStruct(ref ids)
                | &OpCode::ConstructArray(ref ids) => {
                    let result = result.unwrap();
                    target.construct_multi(block_result, result.id, result.type_id, ids)
                }

                &OpCode::Load(pointer) => target.load(block_result, result.unwrap().id, pointer),
                &OpCode::Store(pointer, value) => {
                    debug_assert!(result.is_none());
                    target.store(block_result, pointer, value);
                }

                &OpCode::Call(function_id, ref params) => {
                    target.call(block_result, result.map(|id| id.id), function_id, params)
                }

                &OpCode::Unary(unary_op, id) => {
                    target.unary(block_result, result.unwrap().id, unary_op, id)
                }
                &OpCode::Binary(binary_op, lhs, rhs) => {
                    target.binary(block_result, result.unwrap().id, binary_op, lhs, rhs)
                }
                &OpCode::BuiltIn(built_in_op, ref params) => {
                    target.built_in(block_result, result.map(|id| id.id), built_in_op, params)
                }
                &OpCode::Texture(TextureOpCode::Implicit { is_proj, offset }, sampler, coord) => {
                    target.texture(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        is_proj,
                        offset,
                    )
                }
                &OpCode::Texture(TextureOpCode::Compare { compare }, sampler, coord) => target
                    .texture_compare(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        compare,
                    ),
                &OpCode::Texture(TextureOpCode::Lod { is_proj, lod, offset }, sampler, coord) => {
                    target.texture_lod(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        is_proj,
                        lod,
                        offset,
                    )
                }
                &OpCode::Texture(TextureOpCode::CompareLod { compare, lod }, sampler, coord) => {
                    target.texture_compare_lod(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        compare,
                        lod,
                    )
                }
                &OpCode::Texture(TextureOpCode::Bias { is_proj, bias, offset }, sampler, coord) => {
                    target.texture_bias(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        is_proj,
                        bias,
                        offset,
                    )
                }
                &OpCode::Texture(TextureOpCode::CompareBias { compare, bias }, sampler, coord) => {
                    target.texture_compare_bias(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        compare,
                        bias,
                    )
                }
                &OpCode::Texture(
                    TextureOpCode::Grad { is_proj, dx, dy, offset },
                    sampler,
                    coord,
                ) => target.texture_grad(
                    &self.ir.meta,
                    block_result,
                    result.unwrap().id,
                    sampler,
                    coord,
                    is_proj,
                    dx,
                    dy,
                    offset,
                ),
                &OpCode::Texture(TextureOpCode::Gather { offset }, sampler, coord) => target
                    .texture_gather(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        offset,
                    ),
                &OpCode::Texture(
                    TextureOpCode::GatherComponent { component, offset },
                    sampler,
                    coord,
                ) => target.texture_gather_component(
                    &self.ir.meta,
                    block_result,
                    result.unwrap().id,
                    sampler,
                    coord,
                    component,
                    offset,
                ),
                &OpCode::Texture(TextureOpCode::GatherRef { refz, offset }, sampler, coord) => {
                    target.texture_gather_ref(
                        &self.ir.meta,
                        block_result,
                        result.unwrap().id,
                        sampler,
                        coord,
                        refz,
                        offset,
                    )
                }
                _ => panic!("Internal error: unexpected op when generating AST"),
            }
        });
    }

    fn generate_branch_instruction<T: Target>(
        &self,
        block_result: &mut T::BlockResult,
        op: &OpCode,
        loop_condition_block_result: Option<T::BlockResult>,
        block1_result: Option<T::BlockResult>,
        block2_result: Option<T::BlockResult>,
        case_block_results: Vec<T::BlockResult>,
        target: &mut T,
    ) {
        match op {
            &OpCode::Discard => target.branch_discard(block_result),
            &OpCode::Return(id) => target.branch_return(block_result, id),
            &OpCode::Break => target.branch_break(block_result),
            &OpCode::Continue => target.branch_continue(block_result),
            &OpCode::If(id) => target.branch_if(block_result, id, block1_result, block2_result),
            &OpCode::Loop => {
                target.branch_loop(block_result, loop_condition_block_result, block1_result)
            }
            &OpCode::DoLoop => target.branch_do_loop(block_result, block1_result),
            &OpCode::LoopIf(id) => target.branch_loop_if(block_result, id),
            &OpCode::Switch(id, ref case_ids) => {
                target.branch_switch(block_result, id, case_ids, case_block_results)
            }
            // Passthrough, NextBlock and Merge translate to nothing in GLSL
            &OpCode::Passthrough | &OpCode::NextBlock | &OpCode::Merge(None) => (),
            // Merge with a value should have been eliminated by astify.
            &OpCode::Merge(Some(_)) => panic!(
                "Internal error: unexpected merge instruction with value when generating AST"
            ),
            _ => panic!("Internal error: unexpected branch op when generating AST"),
        }
    }
}
