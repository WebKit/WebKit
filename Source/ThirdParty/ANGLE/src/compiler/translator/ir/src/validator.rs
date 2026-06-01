// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper to validate the rules of IR.  This is useful particularly to be run after
// transformations, to ensure they generate valid IR.
//
// TODO(http://anglebug.com/349994211): to validate:
//
//   - Every ID must be present in the respective map.
//   - Every variable must be defined somewhere, either in global block or in a block.
//   - Every accessed variable must be declared in an accessible block.
//   - Branches must have the appropriate targets set (merge, trueblock for if, etc).
//   - No branches inside a block, every block ends in branch. (i.e. no dead code)
//   - For merge blocks that have an input, the branch instruction of blocks that jump to it have an
//     output.
//   - No identical types with different IDs.
//   - If there's a cached "has side effect", that it's correct.
//   - Validate that ImageType fields are valid in combination with ImageDimension
//   - No operations should have entirely constant arguments, that should be folded (and
//     transformations shouldn't retintroduce it)
//   - Catch misuse of built-in names.
//   - Precision is not applied to types that don't are not applicable.  It _is_ applied to types
//     that are applicable (including uniforms and samplers for example).  Needs to work to make
//     sure precision is always assigned.
//   - Case values are always ConstantId (int/uint only too?)
//   - Variables are Pointers
//   - Pointers only valid in the left arg of load/store/access/call
//   - Loop blocks ends in the appropriate instructions.
//   - Do blocks end in DoLoop (unless already terminated by something else, like Return)
//   - If condition is a bool.
//   - Maximum one default case for Switch instructions.
//   - No pointer->pointer type.
//   - Interface variables with NameSource::Internal are unique.
//   - NameSource::Internal names don't start with the user and temporary name prefixes (_u, t and f
//     respectively).
//   - Interface variables with NameSource::ShaderInterface are unique.
//   - NameSource::ShaderInterface and NameSource::Internal are never found inside body
//   - blocks, those should always be Temporary.
//   - No identity swizzles.
//   - Type matches?
//   - Block inputs have MergeInput opcode, nothing else has that opcode.
//   - Block inputs are not pointers.  AST-ifier does not handle that.
//   - Whatever else is in the AST validation currently.
//   - Validate built-ins that accept an out or inout parameter, that the corresponding parameter is
//     passed a Pointer at the call site.  For that matter, do the same for user function calls too.
//   - Check for invalid texture* combinations, like non-shadow samplers with TextureCompare ops, or
//     is_proj is false for cubemaps.
//   - Check that function parameter variables don't have an initializer.
//   - Check that returned values from functions match the type of the function's return value.
//   - Validate that if there's a merge variable in an if block, both true and false blocks exist
//     and they both end in a Merge with an ID.  (Technically, we should be able to also support one
//     block being merge and the other discard/return/break/continue, but no such code can be
//     generated right now).

#[macro_export]
macro_rules! validate_in_debug_build_only {
    ($arg:expr) => {
        #[cfg(debug_assertions)]
        $crate::validator::validate($arg);
    };
}

#[cfg(debug_assertions)]
use crate::debug;
#[cfg(debug_assertions)]
use crate::ir::*;
#[cfg(debug_assertions)]
use crate::traverser;
#[cfg(debug_assertions)]
use std::fmt;

#[cfg(debug_assertions)]
pub fn validate(ir: &IR) {
    let validator = Validator::new(ir);
    validator.validate();
}

// Validator takes a reference of IR object, and its' lifetime is the same as the lifetime of IR
// object
#[cfg(debug_assertions)]
struct Validator<'a> {
    ir: &'a IR,
    max_type_count: u32,
    max_variable_count: u32,
    max_constant_count: u32,
    max_register_count: u32,
}

#[cfg(debug_assertions)]
impl<'a> Validator<'a> {
    // Validator constructor
    fn new(ir: &'a IR) -> Validator<'a> {
        Validator {
            ir: ir,
            max_type_count: ir.meta.all_types().len() as u32,
            max_variable_count: ir.meta.all_variables().len() as u32,
            max_constant_count: ir.meta.all_constants().len() as u32,
            max_register_count: ir.meta.total_register_count(),
        }
    }

    // ANGLE IR validation entry point
    fn validate(&self) {
        self.validate_all_ids_are_present();
    }

    fn validate_all_ids_are_present(&self) {
        self.validate_all_ids_are_present_in_ir_meta();
        self.validate_all_ids_are_present_in_ir_function_entries();
    }

    fn validate_all_ids_are_present_in_ir_meta(&self) {
        // validate IRMeta.constants
        for (constant_index, constant) in self.ir.meta.all_constants().iter().enumerate() {
            self.validate_all_ids_in_a_constant_are_present(
                constant_index.try_into().unwrap(),
                constant,
            );
        }
        // validate IRMeta.variables
        for (variable_index, variable) in self.ir.meta.all_variables().iter().enumerate() {
            self.validate_all_ids_in_a_variable_are_present(
                variable_index.try_into().unwrap(),
                variable,
            );
        }
        // validate IRMeta.functions
        for (function_index, function) in self.ir.meta.all_functions().iter().enumerate() {
            self.validate_all_ids_in_a_function_are_present(
                function_index.try_into().unwrap(),
                function,
            );
        }

        // Validate IRMeta.global_variables
        for (global_variable_index, global_variable) in
            self.ir.meta.all_global_variables().iter().enumerate()
        {
            if global_variable.id >= self.max_variable_count {
                self.on_error(format_args!(
                    "invalid IRMeta: global_variable {global_variable_index} uses invalid \
                     variable id {}",
                    global_variable.id
                ));
            }
        }

        // validate IRMeta.variables_pending_zero_initialization
        for (variable_pending_zero_initialization_index, variable_id) in
            self.ir.meta.get_variables_pending_zero_initialization().iter().enumerate()
        {
            if variable_id.id >= self.max_variable_count {
                self.on_error(format_args!(
                    "invalid IRMeta: variable_pending_zero_initialization \
                     {variable_pending_zero_initialization_index} uses invalid variable id {}",
                    variable_id.id
                ));
            }
        }
    }

    fn validate_all_ids_are_present_in_ir_function_entries(&self) {
        traverser::visitor::for_each_function(
            &mut (), // no state to track while traversing all functions
            &self.ir.function_entries,
            |_, _| {}, // do nothing in pre_visit
            |_, block, _, _| {
                self.validate_all_ids_are_present_in_block(block);
                traverser::visitor::VISIT_SUB_BLOCKS
            }, // validate ids in a block during visit
            |_, _| {}, // do nothing in post_visit
        );
    }

    // Helper function to check Constant member type_id is valid
    fn validate_all_ids_in_a_constant_are_present(&self, constant_id: u32, constant: &Constant) {
        if constant.type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "Constant id {constant_id} has invalid type id {}",
                constant.type_id.id
            ));
        }
    }

    // Helper function to check Variable members type_id, initializer are valid
    fn validate_all_ids_in_a_variable_are_present(&self, variable_id: u32, variable: &Variable) {
        // Check type_id
        if variable.type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "Variable id {variable_id} has invalid type id {}",
                variable.type_id.id
            ));
        }
        // Check initializer
        if let Some(valid_initializer) = variable.initializer {
            if valid_initializer.id >= self.max_constant_count {
                self.on_error(format_args!(
                    "Variable id {variable_id} has invalid constant initializer {}",
                    valid_initializer.id
                ));
            }
        }
    }

    // Helper function to check Function members return_type_id, params are valid
    fn validate_all_ids_in_a_function_are_present(&self, function_id: u32, function: &Function) {
        // Check return type
        if function.return_type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "Function id {function_id} has invalid return type {}",
                function.return_type_id.id
            ));
        }

        // Check function parameters
        for param in &function.params {
            if param.variable_id.id >= self.max_variable_count {
                self.on_error(format_args!(
                    "Function id {function_id} has invalid parameters {}",
                    param.variable_id.id
                ));
            }
        }
    }

    // TODO yuxinhu@google.com
    // Write a helper function that walks through a block once and collect all information
    // needed for validations

    fn validate_all_ids_are_present_in_block(&self, block: &Block) {
        // validate block variables
        for variable in &block.variables {
            if variable.id >= self.max_variable_count {
                self.on_error(format_args!(
                    "invalid variable id {} found in block variabls",
                    variable.id
                ));
            }
        }
        // validate input
        if let Some(valid_input) = block.input {
            self.validate_block_input_has_valid_ids(&valid_input);
        }
        // validate instructions
        for instruction in &block.instructions {
            let (opcode, result) = instruction.get_op_and_result(&self.ir.meta);
            self.validate_instruction_op_code_typed_id_parameters(opcode);
            if let Some(instruction_result) = result {
                self.validate_opcode_instruction_result_has_valid_ids(opcode, &instruction_result);
            }
        }
    }

    // Validate OpCode parameters
    fn validate_instruction_op_code_typed_id_parameters(&self, op_code: &OpCode) {
        match op_code {
            // OpCode that does not take any parameters: do nothing
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
            // OpCode that takes in Vec<TypedId> params, verify Vec<TypedId> params
            OpCode::Call(_, params)
            | OpCode::ConstructVectorFromMultiple(params)
            | OpCode::ConstructMatrixFromMultiple(params)
            | OpCode::ConstructStruct(params)
            | OpCode::ConstructArray(params)
            | OpCode::BuiltIn(_, params) => {
                for param in params {
                    self.validate_opcode_instruction_typed_id_params(op_code, param);
                }
            }
            // OpCode that takes in TypedId params, verify TypedId
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
            | OpCode::Alias(id)
            | OpCode::Unary(_, id) => {
                self.validate_opcode_instruction_typed_id_params(op_code, id);
            }
            // OpCode that takes two TypedId, verify both TypedId
            OpCode::ExtractVectorComponentDynamic(lhs, rhs)
            | OpCode::ExtractMatrixColumn(lhs, rhs)
            | OpCode::ExtractArrayElement(lhs, rhs)
            | OpCode::AccessVectorComponentDynamic(lhs, rhs)
            | OpCode::AccessMatrixColumn(lhs, rhs)
            | OpCode::AccessArrayElement(lhs, rhs)
            | OpCode::Store(lhs, rhs)
            | OpCode::Binary(_, lhs, rhs) => {
                self.validate_opcode_instruction_typed_id_params(op_code, lhs);
                self.validate_opcode_instruction_typed_id_params(op_code, rhs);
            }
            // OpCode that takes Another OpCode (texture_op) as Parameter
            OpCode::Texture(texture_op, sampler, coord) => {
                self.validate_textureopcode_instruction_typed_id_params(texture_op, sampler);
                self.validate_textureopcode_instruction_typed_id_params(texture_op, coord);
                match texture_op {
                    TextureOpCode::Implicit { is_proj: _, offset }
                    | TextureOpCode::Gather { offset } => {
                        if let Some(valid_offset) = offset {
                            self.validate_textureopcode_instruction_typed_id_params(
                                texture_op,
                                valid_offset,
                            );
                        }
                    }
                    TextureOpCode::Compare { compare } => {
                        self.validate_textureopcode_instruction_typed_id_params(
                            texture_op, compare,
                        );
                    }
                    TextureOpCode::Lod { is_proj: _, lod, offset } => {
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, lod);

                        if let Some(valid_offset) = offset {
                            self.validate_textureopcode_instruction_typed_id_params(
                                texture_op,
                                valid_offset,
                            );
                        }
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        self.validate_textureopcode_instruction_typed_id_params(
                            texture_op, compare,
                        );
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, lod);
                    }
                    TextureOpCode::Bias { is_proj: _, bias, offset } => {
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, bias);
                        if let Some(valid_offset) = offset {
                            self.validate_textureopcode_instruction_typed_id_params(
                                texture_op,
                                valid_offset,
                            );
                        }
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        self.validate_textureopcode_instruction_typed_id_params(
                            texture_op, compare,
                        );
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, bias);
                    }
                    TextureOpCode::Grad { is_proj: _, dx, dy, offset } => {
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, dx);
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, dy);
                        if let Some(valid_offset) = offset {
                            self.validate_textureopcode_instruction_typed_id_params(
                                texture_op,
                                valid_offset,
                            );
                        }
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        self.validate_textureopcode_instruction_typed_id_params(
                            texture_op, component,
                        );
                        if let Some(valid_offset) = offset {
                            self.validate_textureopcode_instruction_typed_id_params(
                                texture_op,
                                valid_offset,
                            );
                        }
                    }
                    TextureOpCode::GatherRef { refz, offset } => {
                        self.validate_textureopcode_instruction_typed_id_params(texture_op, refz);
                        if let Some(valid_offset) = offset {
                            self.validate_textureopcode_instruction_typed_id_params(
                                texture_op,
                                valid_offset,
                            );
                        }
                    }
                }
            }
        };
    }

    // Helper function to check the OpCode instruction result TypedRegisterId contains valid
    // RegisterId and TypeId members
    fn validate_opcode_instruction_result_has_valid_ids(
        &self,
        op_code: &OpCode,
        result: &TypedRegisterId,
    ) {
        if result.id.id >= self.max_register_count {
            self.on_error(format_args!(
                "invalid {:?} instruction result register id {}",
                op_code, result.id.id
            ));
        }
        if result.type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "invalid {:?} instruction result type {}",
                op_code, result.type_id.id
            ));
        }
    }

    // Helper function to check Block input TypedRegisterId contains valid RegisterId and TypeId
    // members
    fn validate_block_input_has_valid_ids(&self, input: &TypedRegisterId) {
        if input.id.id >= self.max_register_count {
            self.on_error(format_args!(
                "invalid block input found, invalid input register id {}",
                input.id.id
            ));
        }
        if input.type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "invalid block input found, invalid input type {}",
                input.type_id.id
            ));
        }
    }

    // Helper function to check OpCode instruction TypedId parameters contain valid id and
    // type_id members
    fn validate_opcode_instruction_typed_id_params(&self, op_code: &OpCode, typed_id: &TypedId) {
        // validate id
        match typed_id.id {
            Id::Register(register_id) => {
                if register_id.id >= self.max_register_count {
                    self.on_error(format_args!(
                        "invalid {:?} instruction: invalid register id {}",
                        op_code, register_id.id
                    ));
                }
            }
            Id::Constant(constant_id) => {
                if constant_id.id >= self.max_constant_count {
                    self.on_error(format_args!(
                        "invalid {:?} instruction: invalid constant id {}",
                        op_code, constant_id.id
                    ));
                }
            }
            Id::Variable(variable_id) => {
                if variable_id.id >= self.max_variable_count {
                    self.on_error(format_args!(
                        "invalid {:?} instruction: invalid variable id {}",
                        op_code, variable_id.id
                    ));
                }
            }
        }
        // validate typed_id
        if typed_id.type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "invalid {:?} instruction: invalid type id {}",
                op_code, typed_id.type_id.id
            ));
        }
    }

    // Helper function to check TextureOpCode instruction TypedId parameters contain valid id
    // and type_id members
    fn validate_textureopcode_instruction_typed_id_params(
        &self,
        op_code: &TextureOpCode,
        typed_id: &TypedId,
    ) {
        // validate id
        match typed_id.id {
            Id::Register(register_id) => {
                if register_id.id >= self.max_register_count {
                    self.on_error(format_args!(
                        "invalid {:?} instruction: invalid register id {}",
                        op_code, register_id.id
                    ));
                }
            }
            Id::Constant(constant_id) => {
                if constant_id.id >= self.max_constant_count {
                    self.on_error(format_args!(
                        "invalid {:?} instruction: invalid constant id {}",
                        op_code, constant_id.id
                    ));
                }
            }
            Id::Variable(variable_id) => {
                if variable_id.id >= self.max_variable_count {
                    self.on_error(format_args!(
                        "invalid {:?} instruction: invalid variable id {}",
                        op_code, variable_id.id
                    ));
                }
            }
        }
        // validate typed_id
        if typed_id.type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "invalid {:?} instruction: invalid type id {}",
                op_code, typed_id.type_id.id
            ));
        }
    }

    // Helper Function to print the invalid IR and then panic!
    fn on_error(&self, validation_error_msg: fmt::Arguments) {
        println!("Internal error: Invalid ANGLE IR! {}", validation_error_msg);
        debug::dump(&self.ir);
        panic!();
    }
}
