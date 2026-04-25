// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper to validate the rules of IR.  This is useful particularly to be run after
// transformations, to ensure they generate valid IR.
//
// Validations implemented:
//
// IDs and scopes:
//   - Every ID must be present in the respective map: validate_all_ids_are_present()
//   - Every variable must be defined somewhere, either in global block or in a block:
//     validate_all_variables_are_declared_in_scope()
//   - Every accessed variable must be declared in an accessible block:
//     validate_all_variables_are_declared_in_scope()
//   - Every accessed register must be declared in an accessible block:
//     validate_all_registers_are_declared_in_scope()
//   - No identical types with different IDs: validate_no_identical_types_with_different_ids()
//   - Referenced IDs are not dead-code-eliminated: validate_data_type_id_is_in_bound_and_alive(),
//     validate_constant_id_is_in_bound_and_alive(), validate_variable_id_is_in_bound_and_alive()
//
// Types:
//   - Validate that ImageType fields are valid in combination with ImageDimension:
//     validate_image_types()
//   - Variables are Pointers: validate_all_alive_variables_are_pointers()
//   - No pointer->pointer type: validate_no_pointer_to_pointer_type()
//
// Control flow:
//   - Branches must have the appropriate targets set (merge, trueblock for if, etc); every block
//     ends in branch: validate_all_branch_instructions_have_valid_target()
//   - No branches inside a block (i.e. no dead code): validate_no_dead_code()
//   - For merge blocks that have an input, the branch instruction of blocks that jump to it have an
//     output: validate_merge_block_with_input()
//   - For block that has a merge block with an input, the branch instruction must be If, and the
//     block must contains block1: validate_merge_block_with_input()
//
// Instructions:
//   - Access to struct fields are in bounds: validate_struct_field_in_bounds()
//   - If conditions are boolean: validate_if_condition_is_bool()
//   - No operations should have entirely constant arguments, that should be folded (and
//     transformations shouldn't retintroduce it): validate_no_constant_foldable_instruction()
//   - Case values are always ConstantId, and the Constant data type is int or uint:
//     validate_case_values_are_int_or_uint_constants()
//   - No duplicated case values for Switch opcode: validate_switch_has_unique_case_values()
//   - Block inputs have MergeInput opcode, nothing else has that opcode:
//     validate_merge_block_input_prerequisites(),
//     validate_no_merge_input_opcode_in_block_instruction()

// TODO(http://anglebug.com/349994211): to validate:
//   - If there's a cached "has side effect", that it's correct.
//   - Catch misuse of built-in names.
//   - Precision is not applied to types that don't are not applicable.  It _is_ applied to types
//     that are applicable (including uniforms and samplers for example).  Needs to work to make
//     sure precision is always assigned.
//   - Pointers only valid in the left arg of load/store/access/call
//   - Arguments of OpCode that must be pointer type is indeed a pointer
//   - Loop blocks ends in the appropriate instructions.
//   - Do blocks end in DoLoop (unless already terminated by something else, like Return)
//   - Interface variables with NameSource::Internal are unique.
//   - NameSource::Internal names don't start with the user and temporary name prefixes (_u, t and f
//     respectively).
//   - Interface variables with NameSource::ShaderInterface are unique.
//   - NameSource::ShaderInterface and NameSource::Internal are never found inside body
//   - blocks, those should always be Temporary.
//   - No identity swizzles.
//   - Type matches?
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
//   - Images with the Rect dimension can only have a Float base type and be 2D samplers (not
//     storage image, array, msaa, etc).
//   - Between the Smooth, Flat, NoPerspective, Centroid and Sample decorations, they are all
//     mutually exclusive, except NoPerspective can be combined with Centroid or Sample.  See
//     ffi::Interpolation in reflection.rs for reference.
//   - ReadOnly and WriteOnly decorations are mutually exclusive.

use crate::ir::*;
use crate::*;
use std::fmt;

pub fn validate(ir: &IR) {
    let validator = Validator::new(ir);
    validator.validate();
}

#[derive(Copy, Clone, PartialEq)]
enum TypedIdValidationCategory {
    // check id does not exceed max constant_id, max register_id, max variable_id
    IdInBound,
    // check variabld_id is declared in the current accessible scope
    VariableDeclared,
    // check register_id is declared in the current accessible scope
    RegisterDeclared,
}

struct DeclaredVarTracker {
    declared_vars_in_current_scope: Vec<HashSet<u32>>,
}

impl DeclaredVarTracker {
    fn new() -> DeclaredVarTracker {
        DeclaredVarTracker { declared_vars_in_current_scope: Vec::new() }
    }

    fn set_global_declared_vars(&mut self, ir_meta_global_vars: &Vec<VariableId>) {
        // global_vars should be the first hash set to be added into the
        // declared_vars_in_current_scope
        debug_assert!(self.declared_vars_in_current_scope.is_empty());
        let mut global_vars = HashSet::new();
        for global_var in ir_meta_global_vars {
            global_vars.insert(global_var.id);
        }
        self.declared_vars_in_current_scope.push(global_vars);
    }

    // Some variables are declared in the function parameters.
    // For example:
    // void my_function(int function_param_var)
    // {
    //   // do something with function_param_var
    // }
    // function_param_var is declared in the function parameter
    fn add_function_param_vars_upon_enter_function(
        &mut self,
        function_parameters: &Vec<FunctionParam>,
    ) {
        // global_vars should be the only hash set in declared_vars_in_current_scope before we add
        // current function param variables
        debug_assert!(self.declared_vars_in_current_scope.len() == 1);
        let mut function_param_vars = HashSet::new();
        for function_param in function_parameters {
            function_param_vars.insert(function_param.variable_id.id);
        }
        self.declared_vars_in_current_scope.push(function_param_vars);
    }

    fn remove_function_param_vars_upon_exit_function(&mut self) {
        self.declared_vars_in_current_scope.pop().unwrap();
        // global_vars should be the only hash set in declared_vars_in_current_scope after we pop
        // current function param variables
        debug_assert!(self.declared_vars_in_current_scope.len() == 1);
    }

    fn add_current_scope_declared_vars_upon_enter_scope(
        &mut self,
        parent_declared_vars: &Vec<VariableId>,
    ) {
        let mut parent_declared_var_map = HashSet::new();

        for parent_var in parent_declared_vars {
            parent_declared_var_map.insert(parent_var.id);
        }

        self.declared_vars_in_current_scope.push(parent_declared_var_map);
    }

    fn remove_current_scope_declared_vars_upon_exit_scope(&mut self) {
        self.declared_vars_in_current_scope.pop().unwrap();
    }

    fn is_variable_declared(&self, variable_id: VariableId) -> bool {
        for declared_var_map in &self.declared_vars_in_current_scope {
            if declared_var_map.contains(&variable_id.id) {
                return true;
            }
        }
        return false;
    }
}

struct DeclaredRegisterTracker {
    declared_registers_in_current_scope: Vec<HashSet<RegisterId>>,
}

impl DeclaredRegisterTracker {
    fn new() -> DeclaredRegisterTracker {
        DeclaredRegisterTracker { declared_registers_in_current_scope: Vec::new() }
    }

    fn add_scope(&mut self) {
        self.declared_registers_in_current_scope.push(HashSet::new());
    }

    fn remove_scope(&mut self) {
        self.declared_registers_in_current_scope.pop().unwrap();
    }

    fn declare_register<'a>(&mut self, register_id: RegisterId, validator: &Validator<'a>) {
        // first check we have not declared this register yet
        if self.is_declared(register_id) {
            validator.on_error(format_args!(
                "register {} is already declared, can't declare the same register twice",
                register_id.id
            ));
        }
        // add the register to the declaration map
        self.declared_registers_in_current_scope.last_mut().unwrap().insert(register_id);
    }

    fn is_declared(&self, register_id: RegisterId) -> bool {
        for declared_registers in self.declared_registers_in_current_scope.iter().rev() {
            if declared_registers.contains(&register_id) {
                return true;
            }
        }
        false
    }
}

#[derive(Clone)]
struct BlockMetaData {
    can_break: bool,
    can_continue: bool,
    can_passthrough: bool,
    is_loop: bool,
    has_merge_block: bool,
    switch_case_count: i32,
    case_block_index: i32,
}

impl BlockMetaData {
    fn new() -> BlockMetaData {
        BlockMetaData {
            can_break: false,
            can_continue: false,
            can_passthrough: false,
            is_loop: false,
            has_merge_block: false,
            switch_case_count: 0,
            case_block_index: -1,
        }
    }
}

// Validator takes a reference of IR object, and its' lifetime is the same as the lifetime of IR
// object
struct Validator<'a> {
    ir: &'a IR,
    max_type_count: u32,
    max_variable_count: u32,
    max_constant_count: u32,
    max_register_count: u32,
}

impl<'a> Validator<'a> {
    // Validator constructor
    fn new(ir: &'a IR) -> Validator<'a> {
        Validator {
            ir,
            max_type_count: ir.meta.all_types().len() as u32,
            max_variable_count: ir.meta.all_variables().len() as u32,
            max_constant_count: ir.meta.all_constants().len() as u32,
            max_register_count: ir.meta.total_register_count(),
        }
    }

    // ANGLE IR validation entry point
    fn validate(&self) {
        self.validate_all_ids_are_present();
        self.validate_all_alive_variables_are_pointers();
        self.validate_no_pointer_to_pointer_type();
        self.validate_all_variables_are_declared_in_scope();
        self.validate_all_registers_are_declared_in_scope();
        self.validate_no_identical_types_with_different_ids();
        self.validate_image_types();
        self.validate_no_dead_code();
        self.validate_all_branch_instructions_have_valid_target();
        self.validate_merge_block_with_input();
        self.validate_all_instructions();
    }

    fn validate_all_ids_are_present(&self) {
        self.validate_all_ids_are_present_in_ir_meta();
        self.validate_all_ids_are_present_in_ir_function_entries();
    }

    fn validate_all_ids_are_present_in_ir_meta(&self) {
        // validate IRMeta.constants
        for (constant_id, constant) in self.ir.meta.all_constants().iter().enumerate() {
            if !constant.is_dead_code_eliminated {
                self.validate_all_ids_in_a_constant_are_present(
                    constant_id.try_into().unwrap(),
                    constant,
                );
            }
        }
        // validate IRMeta.variables
        for (variable_id, variable) in self.ir.meta.all_variables().iter().enumerate() {
            if !variable.is_dead_code_eliminated {
                self.validate_all_ids_in_a_variable_are_present(
                    variable_id.try_into().unwrap(),
                    variable,
                );
            }
        }
        // validate IRMeta.functions
        traverser::visitor::for_each_function(
            &mut (),
            &self.ir.function_entries,
            |_, function_id| {
                let function = self.ir.meta.get_function(function_id);
                self.validate_all_ids_in_a_function_are_present(function_id.id, function);
            },
            |_, _, _, _| traverser::visitor::STOP,
            |_, _| {},
        );

        // Validate IRMeta.global_variables
        for global_variable_id in self.ir.meta.all_global_variables() {
            self.validate_variable_id_is_in_bound_and_alive(
                *global_variable_id,
                format_args!("global_variable {:?}", global_variable_id),
            );
        }

        // validate IRMeta.variables_pending_zero_initialization
        for variable_id in self.ir.meta.get_variables_pending_zero_initialization() {
            self.validate_variable_id_is_in_bound_and_alive(
                *variable_id,
                format_args!("variable_pending_zero_initialization {:?}", variable_id),
            );
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
        self.validate_data_type_id_is_in_bound_and_alive(
            constant.type_id,
            format_args!("constant {constant_id}"),
        );
    }

    // Helper function to check Variable members type_id, initializer are valid
    fn validate_all_ids_in_a_variable_are_present(&self, variable_id: u32, variable: &Variable) {
        // Check type_id
        self.validate_data_type_id_is_in_bound_and_alive(
            variable.type_id,
            format_args!("variable {variable_id}"),
        );
        // Check initializer
        if let Some(valid_initializer) = variable.initializer {
            self.validate_constant_id_is_in_bound_and_alive(
                valid_initializer,
                format_args!("variable {variable_id} initializer"),
            );
        }
    }

    // Helper function to check Function members return_type_id, params are valid
    fn validate_all_ids_in_a_function_are_present(&self, function_id: u32, function: &Function) {
        // Check return type
        self.validate_data_type_id_is_in_bound_and_alive(
            function.return_type_id,
            format_args!("return type of function {function_id}"),
        );

        // Check function parameters
        for param in &function.params {
            self.validate_variable_id_is_in_bound_and_alive(
                param.variable_id,
                format_args!("function {function_id} parameter"),
            );
        }
    }

    // TODO yuxinhu@google.com
    // Write a helper function that walks through a block once and collect all information
    // needed for validations

    fn validate_all_ids_are_present_in_block(&self, block: &Block) {
        // validate block variables
        for variable in &block.variables {
            self.validate_variable_id_is_in_bound_and_alive(
                *variable,
                format_args!("block variables"),
            );
        }
        // validate input
        if let Some(valid_input) = block.input {
            self.validate_block_input_has_valid_ids(&valid_input);
        }
        // validate instructions
        for instruction in &block.instructions {
            let (opcode, result) = instruction.get_op_and_result(&self.ir.meta);
            self.validate_instruction_op_code_typed_id_parameters(
                opcode,
                TypedIdValidationCategory::IdInBound,
                None,
                None,
            );
            if let Some(instruction_result) = result {
                self.validate_opcode_instruction_result_has_valid_ids(opcode, &instruction_result);
            }
        }
    }

    fn validate_data_type_id_is_in_bound_and_alive(
        &self,
        type_id: TypeId,
        context: fmt::Arguments,
    ) {
        if type_id.id >= self.max_type_count {
            self.on_error(format_args!(
                "invalid TypeId found in {context}, TypeId {} is out of bound",
                type_id.id
            ));
        }
        if self.ir.meta.all_types()[type_id.id as usize].is_dead_code_eliminated() {
            self.on_error(format_args!(
                "invalid TypeId found in {context}, TypeId {} is dead code eliminated",
                type_id.id
            ));
        }
    }

    fn validate_constant_id_is_in_bound_and_alive(
        &self,
        constant_id: ConstantId,
        context: fmt::Arguments,
    ) {
        if constant_id.id >= self.max_constant_count {
            self.on_error(format_args!(
                "invalid ConstantId found in {context}, ConstantId {} is out of bound",
                constant_id.id
            ));
        }
        if self.ir.meta.all_constants()[constant_id.id as usize].is_dead_code_eliminated {
            self.on_error(format_args!(
                "invalid ConstantId found in {context}, ConstantId {} is dead code eliminated",
                constant_id.id
            ));
        }
    }

    fn validate_variable_id_is_in_bound_and_alive(
        &self,
        variable_id: VariableId,
        context: fmt::Arguments,
    ) {
        if variable_id.id >= self.max_variable_count {
            self.on_error(format_args!(
                "invalid VariableId found in {context}, VariableId {} is out of bound",
                variable_id.id
            ));
        }
        if self.ir.meta.all_variables()[variable_id.id as usize].is_dead_code_eliminated {
            self.on_error(format_args!(
                "invalid VariableId found in {context}, VariableId {} is dead code eliminated",
                variable_id.id
            ));
        }
    }

    // Validate OpCode parameters
    fn validate_instruction_op_code_typed_id_parameters(
        &self,
        op_code: &OpCode,
        category: TypedIdValidationCategory,
        declared_variables: Option<&DeclaredVarTracker>,
        declared_registers: Option<&DeclaredRegisterTracker>,
    ) {
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
                    self.validate_typed_id_params(
                        op_code,
                        param,
                        category,
                        declared_variables,
                        declared_registers,
                    );
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
                self.validate_typed_id_params(
                    op_code,
                    id,
                    category,
                    declared_variables,
                    declared_registers,
                );
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
                self.validate_typed_id_params(
                    op_code,
                    lhs,
                    category,
                    declared_variables,
                    declared_registers,
                );
                self.validate_typed_id_params(
                    op_code,
                    rhs,
                    category,
                    declared_variables,
                    declared_registers,
                );
            }
            // OpCode that takes Another OpCode (texture_op) as Parameter
            OpCode::Texture(texture_op, sampler, coord) => {
                self.validate_typed_id_params(
                    texture_op,
                    sampler,
                    category,
                    declared_variables,
                    declared_registers,
                );
                self.validate_typed_id_params(
                    texture_op,
                    coord,
                    category,
                    declared_variables,
                    declared_registers,
                );
                match texture_op {
                    TextureOpCode::Implicit { is_proj: _, offset }
                    | TextureOpCode::Gather { offset } => {
                        if let Some(valid_offset) = offset {
                            self.validate_typed_id_params(
                                texture_op,
                                valid_offset,
                                category,
                                declared_variables,
                                declared_registers,
                            );
                        }
                    }
                    TextureOpCode::Compare { compare } => {
                        self.validate_typed_id_params(
                            texture_op,
                            compare,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                    }
                    TextureOpCode::Lod { is_proj: _, lod, offset } => {
                        self.validate_typed_id_params(
                            texture_op,
                            lod,
                            category,
                            declared_variables,
                            declared_registers,
                        );

                        if let Some(valid_offset) = offset {
                            self.validate_typed_id_params(
                                texture_op,
                                valid_offset,
                                category,
                                declared_variables,
                                declared_registers,
                            );
                        }
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        self.validate_typed_id_params(
                            texture_op,
                            compare,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        self.validate_typed_id_params(
                            texture_op,
                            lod,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                    }
                    TextureOpCode::Bias { is_proj: _, bias, offset } => {
                        self.validate_typed_id_params(
                            texture_op,
                            bias,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        if let Some(valid_offset) = offset {
                            self.validate_typed_id_params(
                                texture_op,
                                valid_offset,
                                category,
                                declared_variables,
                                declared_registers,
                            );
                        }
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        self.validate_typed_id_params(
                            texture_op,
                            compare,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        self.validate_typed_id_params(
                            texture_op,
                            bias,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                    }
                    TextureOpCode::Grad { is_proj: _, dx, dy, offset } => {
                        self.validate_typed_id_params(
                            texture_op,
                            dx,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        self.validate_typed_id_params(
                            texture_op,
                            dy,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        if let Some(valid_offset) = offset {
                            self.validate_typed_id_params(
                                texture_op,
                                valid_offset,
                                category,
                                declared_variables,
                                declared_registers,
                            );
                        }
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        self.validate_typed_id_params(
                            texture_op,
                            component,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        if let Some(valid_offset) = offset {
                            self.validate_typed_id_params(
                                texture_op,
                                valid_offset,
                                category,
                                declared_variables,
                                declared_registers,
                            );
                        }
                    }
                    TextureOpCode::GatherRef { refz, offset } => {
                        self.validate_typed_id_params(
                            texture_op,
                            refz,
                            category,
                            declared_variables,
                            declared_registers,
                        );
                        if let Some(valid_offset) = offset {
                            self.validate_typed_id_params(
                                texture_op,
                                valid_offset,
                                category,
                                declared_variables,
                                declared_registers,
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

        self.validate_data_type_id_is_in_bound_and_alive(
            result.type_id,
            format_args!("result of instruction {:?}", op_code),
        );
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
        self.validate_data_type_id_is_in_bound_and_alive(
            input.type_id,
            format_args!("block input"),
        );
    }

    // Helper function to check OpCode instruction TypedId parameters contain valid id and
    // type_id members
    fn validate_typed_id_params(
        &self,
        op_code: &dyn fmt::Debug,
        typed_id: &TypedId,
        category: TypedIdValidationCategory,
        declared_variables: Option<&DeclaredVarTracker>,
        declared_registers: Option<&DeclaredRegisterTracker>,
    ) {
        // validate id
        match typed_id.id {
            Id::Register(register_id) => {
                match category {
                    TypedIdValidationCategory::IdInBound => {
                        if register_id.id >= self.max_register_count {
                            self.on_error(format_args!(
                                "invalid {:?} instruction: invalid register id {}",
                                op_code, register_id.id
                            ));
                        }
                    }

                    TypedIdValidationCategory::VariableDeclared => {
                        // Do nothing
                    }
                    TypedIdValidationCategory::RegisterDeclared => {
                        let declared_register_tracker = declared_registers.expect(
                            "expecting valid DeclaredRegisterTracker provided for \
                             RegisterDeclared category",
                        );
                        if !declared_register_tracker.is_declared(register_id) {
                            self.on_error(format_args!(
                                "invalid {:?} instruction: undeclared register id {}",
                                op_code, register_id.id
                            ));
                        }
                    }
                }
            }
            Id::Constant(constant_id) => {
                match category {
                    TypedIdValidationCategory::IdInBound => {
                        self.validate_constant_id_is_in_bound_and_alive(
                            constant_id,
                            format_args!("parameter of instruction {:?}", op_code),
                        );
                    }

                    TypedIdValidationCategory::VariableDeclared => {
                        // Do nothing
                    }
                    TypedIdValidationCategory::RegisterDeclared => {
                        // Do nothing
                    }
                }
            }
            Id::Variable(variable_id) => match category {
                TypedIdValidationCategory::IdInBound => {
                    self.validate_variable_id_is_in_bound_and_alive(
                        variable_id,
                        format_args!("parameter of instruction {:?}", op_code),
                    );
                }

                TypedIdValidationCategory::VariableDeclared => {
                    let declared_variables_tracker = declared_variables.expect(
                        "expecting valid DeclaredVarTracker provided for VariableDeclared category",
                    );
                    if !declared_variables_tracker.is_variable_declared(variable_id) {
                        self.on_error(format_args!(
                            "invalid {:?} instruction: undeclared variable id {}",
                            op_code, variable_id.id
                        ));
                    }
                }
                TypedIdValidationCategory::RegisterDeclared => {
                    // Do nothing
                }
            },
        }

        self.validate_data_type_id_is_in_bound_and_alive(
            typed_id.type_id,
            format_args!("parameter of instruction {:?}", op_code),
        );
    }

    // Helper Function to print the invalid IR and then panic!
    fn on_error(&self, validation_error_msg: fmt::Arguments) {
        println!("Internal error: Invalid ANGLE IR! {}", validation_error_msg);
        debug::dump(self.ir);
        panic!();
    }

    fn validate_all_alive_variables_are_pointers(&self) {
        for alive_variable in
            self.ir.meta.all_variables().iter().filter(|variable| !variable.is_dead_code_eliminated)
        {
            if !self.ir.meta.get_type(alive_variable.type_id).is_pointer() {
                self.on_error(format_args!(
                    "invalid variable: variable {:?} is not a pointer",
                    alive_variable
                ));
            }
        }
    }

    fn validate_no_pointer_to_pointer_type(&self) {
        for data_type in self.ir.meta.all_types().iter().filter(|t| !t.is_dead_code_eliminated()) {
            if data_type.is_pointer()
                && self.ir.meta.get_type(data_type.get_element_type_id().unwrap()).is_pointer()
            {
                self.on_error(format_args!(
                    "invalid type: type {:?} is a pointer to a pointer",
                    data_type
                ));
            }
        }
    }

    fn validate_all_variables_are_declared_in_scope(&self) {
        let mut vars_declared_map = DeclaredVarTracker::new();
        vars_declared_map.set_global_declared_vars(self.ir.meta.all_global_variables());

        let current_function = FunctionId { id: 0 };
        let mut state = (current_function, vars_declared_map);
        traverser::visitor::for_each_function(
            &mut state,
            &self.ir.function_entries,
            |(current_function, _), function_id| {
                *current_function = function_id;
            },
            |(current_function, vars_declared_map), entry, _, _| {
                let function = &self.ir.meta.get_function(*current_function);

                vars_declared_map.add_function_param_vars_upon_enter_function(&function.params);
                self.validate_all_variables_in_a_block_are_declared_in_scope(
                    vars_declared_map,
                    entry,
                );
                vars_declared_map.remove_function_param_vars_upon_exit_function();

                traverser::visitor::STOP
            },
            |_, _| {},
        );
    }

    fn validate_all_variables_in_a_block_are_declared_in_scope(
        &self,
        vars_declared_map: &mut DeclaredVarTracker,
        block: &Block,
    ) {
        // push the block variable to vars_declared
        vars_declared_map.add_current_scope_declared_vars_upon_enter_scope(&block.variables);

        // Validate variable used in each instructions
        for instruction in &block.instructions {
            let (opcode, _result) = instruction.get_op_and_result(&self.ir.meta);
            self.validate_instruction_op_code_typed_id_parameters(
                opcode,
                TypedIdValidationCategory::VariableDeclared,
                Some(vars_declared_map),
                None,
            );
        }

        // Check sub blocks, excluding merge_block
        block.for_each_sub_block(vars_declared_map, &|vars_declared_map, sub_block| {
            self.validate_all_variables_in_a_block_are_declared_in_scope(
                vars_declared_map,
                sub_block,
            )
        });

        // Continue check merge_block
        if let Some(valid_merge_block) = &block.merge_block {
            self.validate_all_variables_in_a_block_are_declared_in_scope(
                vars_declared_map,
                valid_merge_block,
            );
        }

        // pop the block variable from vars_declared_map
        vars_declared_map.remove_current_scope_declared_vars_upon_exit_scope();
    }

    fn validate_all_registers_are_declared_in_scope(&self) {
        let mut registers_declared_map = DeclaredRegisterTracker::new();
        for entry in &self.ir.function_entries {
            if entry.is_none() {
                // Skip over functions that have been dead-code eliminated.
                continue;
            }
            self.validate_block_registers(entry.as_ref().unwrap(), &mut registers_declared_map);
        }
    }

    fn validate_block_registers(
        &self,
        block: &Block,
        registers_declared_map: &mut DeclaredRegisterTracker,
    ) {
        registers_declared_map.add_scope();

        block.input.inspect(|input| {
            registers_declared_map.declare_register(input.id, self);
        });

        for instruction in &block.instructions {
            let (opcode, result) = instruction.get_op_and_result(&self.ir.meta);
            self.validate_instruction_op_code_typed_id_parameters(
                opcode,
                TypedIdValidationCategory::RegisterDeclared,
                None,
                Some(registers_declared_map),
            );
            result.inspect(|result| {
                registers_declared_map.declare_register(result.id, self);
            });
        }

        block.for_each_sub_block(registers_declared_map, &|registers_declared_map, sub_block| {
            self.validate_block_registers(sub_block, registers_declared_map);
        });

        block.merge_block.as_ref().inspect(|merge_block| {
            self.validate_block_registers(merge_block, registers_declared_map);
        });

        registers_declared_map.remove_scope();
    }

    fn validate_no_identical_types_with_different_ids(&self) {
        let mut seen_types: HashSet<&ir::Type> = HashSet::new();
        for ir_type in self.ir.meta.all_types() {
            // Some Type will be marked as dead code eliminated during transform, they won't be
            // included in final IR, skip them.
            if ir_type.is_dead_code_eliminated() {
                continue;
            }
            // https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.html#structures
            // Two structure types are the same if they have the same name
            // However, for struct specifier declared as follows:
            // struct
            // {
            //     int field;
            // }s;
            // The GLSL parser code will use empty string for the struct type name.
            // For the following struct declarations:
            // struct
            // {
            //     int field;
            // }s1;
            // struct
            // {
            //     int field;
            // }s2;
            // In IR, we end up with two same ir::Type::Struct(name, fields, struct_specifier),
            // where as we should treat them as different types.
            // Skip the uniqueness check for struct with empty name for now.
            if ir_type.is_struct_with_empty_name() {
                continue;
            }

            // For interface block, right now for the following interface block declarations
            // in Vertex
            // {
            //     ivec4 iv;
            //     vec4  fv;
            // } inVertex[];
            // out Vertex
            // {
            //     ivec4 iv;
            //     vec4  fv;
            // } outVertex[];
            // They end up with the same ir::Type::Struct(name, fields, struct_specifier), but they
            // should be treated as different types.
            // Similar problem arises where the gl_PerVertex is redeclared in geometry shader,
            // IR ends with with two same ir::Type::Struct(name, fields, struct_specifier).
            // Skip the uniqueness check for interface block struct for now.
            if ir_type.is_struct_interface_block() {
                continue;
            }
            if !seen_types.insert(ir_type) {
                self.on_error(format_args!(
                    "identical type {:?} found with different IDs",
                    ir_type
                ));
            }
        }
    }

    fn validate_image_types(&self) {
        for ir_type in self.ir.meta.all_types() {
            if let Type::Image(basic_type, image_type) = ir_type {
                let invalid_combo = match image_type.dimension {
                    ImageDimension::D2 => {
                        if *basic_type == ImageBasicType::Float
                            && image_type.is_sampled
                            && image_type.is_ms
                            && image_type.is_shadow
                        {
                            Some("float 2D multisampled shadow sampler")
                        } else if (*basic_type == ImageBasicType::Int
                            || *basic_type == ImageBasicType::Uint)
                            && image_type.is_sampled
                            && image_type.is_shadow
                        {
                            Some("int 2D shadow sampler or uint 2D shadow sampler")
                        } else if !image_type.is_sampled
                            && (image_type.is_ms || image_type.is_shadow)
                        {
                            Some("2D multisampled storage image or 2D shadow storage image")
                        } else {
                            None
                        }
                    }
                    ImageDimension::D3 => {
                        if image_type.is_array || image_type.is_ms || image_type.is_shadow {
                            Some("3D array, 3D multisampled or 3D shadow image types")
                        } else {
                            None
                        }
                    }
                    ImageDimension::Cube => {
                        if image_type.is_ms {
                            Some("multisampled cube image types")
                        } else if (*basic_type == ImageBasicType::Int
                            || *basic_type == ImageBasicType::Uint)
                            && image_type.is_sampled
                            && image_type.is_shadow
                        {
                            Some("int or uint cube shadow sampler")
                        } else if !image_type.is_sampled && image_type.is_shadow {
                            Some("cube shadow storage image")
                        } else {
                            None
                        }
                    }
                    ImageDimension::External => {
                        if *basic_type == ImageBasicType::Int
                            || *basic_type == ImageBasicType::Uint
                            || !image_type.is_sampled
                            || image_type.is_array
                            || image_type.is_ms
                            || image_type.is_shadow
                        {
                            Some(
                                "int external image, uint external image, storage external image, \
                                 array external image, multismpled external image, shadow \
                                 external image",
                            )
                        } else {
                            None
                        }
                    }
                    ImageDimension::ExternalY2Y => {
                        if *basic_type == ImageBasicType::Int
                            || *basic_type == ImageBasicType::Uint
                            || !image_type.is_sampled
                            || image_type.is_array
                            || image_type.is_ms
                            || image_type.is_shadow
                        {
                            Some(
                                "int external y2y image, uint external y2y image, storage \
                                 external y2y image, array external y2y image, multismpled \
                                 external y2y image, shadow external y2y image",
                            )
                        } else {
                            None
                        }
                    }
                    ImageDimension::Rect => {
                        if !image_type.is_sampled
                            || image_type.is_array
                            || image_type.is_ms
                            || image_type.is_shadow
                        {
                            Some(
                                "storage rect image, array rect image, multisampled rect image, \
                                 shadow rect image",
                            )
                        } else {
                            None
                        }
                    }
                    ImageDimension::Buffer => {
                        if image_type.is_array || image_type.is_ms || image_type.is_shadow {
                            Some(
                                "array buffer image, multisampled buffer image, shadow buffer \
                                 image",
                            )
                        } else {
                            None
                        }
                    }
                    ImageDimension::Video => {
                        if *basic_type == ImageBasicType::Int
                            || *basic_type == ImageBasicType::Uint
                            || !image_type.is_sampled
                            || image_type.is_array
                            || image_type.is_ms
                            || image_type.is_shadow
                        {
                            Some(
                                "int video image, uint video image, storage video image, array \
                                 video image, multisample video image, shadow video image",
                            )
                        } else {
                            None
                        }
                    }
                    ImageDimension::PixelLocal => {
                        if image_type.is_sampled
                            || image_type.is_array
                            || image_type.is_ms
                            || image_type.is_shadow
                        {
                            Some(
                                "pixel local image sampler, array pixel local image, multisample \
                                 pixel local image, shadow pixel local image",
                            )
                        } else {
                            None
                        }
                    }
                    ImageDimension::Subpass => {
                        if image_type.is_sampled
                            || image_type.is_array
                            || image_type.is_ms
                            || image_type.is_shadow
                        {
                            Some(
                                "subpass image sampler, array subpass image, multisample subpass \
                                 image, shadow subpass image",
                            )
                        } else {
                            None
                        }
                    }
                };
                if let Some(invalid_combo) = invalid_combo {
                    self.on_error(format_args!(
                        "invalid image type {:?}, {} is not supported in GLSL",
                        ir_type, invalid_combo
                    ));
                }
            }
        }
    }

    fn validate_no_constant_foldable_instruction(&self, opcode: &OpCode) {
        match opcode {
            OpCode::Binary(_, lhs, rhs) => {
                self.validate_not_all_args_are_constant(opcode, &[*lhs, *rhs]);
            }
            OpCode::Unary(_, param) => {
                self.validate_not_all_args_are_constant(opcode, &[*param]);
            }
            OpCode::BuiltIn(built_in_opcode, params) if built_in_opcode.may_const_fold() => {
                self.validate_not_all_args_are_constant(opcode, params);
            }

            OpCode::ConstructVectorFromMultiple(params)
            | OpCode::ConstructMatrixFromMultiple(params)
            | OpCode::ConstructStruct(params)
            | OpCode::ConstructArray(params) => {
                self.validate_not_all_args_are_constant(opcode, params);
            }

            OpCode::ConstructScalarFromScalar(param)
            | OpCode::ConstructVectorFromScalar(param)
            | OpCode::ConstructMatrixFromScalar(param)
            | OpCode::ConstructMatrixFromMatrix(param)
            | OpCode::ExtractVectorComponent(param, _)
            | OpCode::ExtractVectorComponentMulti(param, _)
            | OpCode::ExtractStructField(param, _) => {
                self.validate_not_all_args_are_constant(opcode, &[*param]);
            }

            OpCode::ExtractVectorComponentDynamic(indexed, index)
            | OpCode::ExtractMatrixColumn(indexed, index)
            | OpCode::ExtractArrayElement(indexed, index) => {
                self.validate_not_all_args_are_constant(opcode, &[*indexed, *index]);
            }

            OpCode::Switch(switch_condition, case_values) => {
                if case_values.is_empty() {
                    self.on_error(format_args!(
                        "operation {:?} could be constant folded. Switch operation has no case \
                         values, Switch instruction should be removed",
                        opcode
                    ));
                }

                if let Id::Constant(switch_condition_constant_id) = switch_condition.id {
                    for case_value_id in case_values.iter().flatten() {
                        if switch_condition_constant_id != *case_value_id {
                            self.on_error(format_args!(
                                "operation {:?} could be constant folded. Case value does not \
                                 match with switch condition constant, case should be removed",
                                opcode
                            ));
                        }
                    }
                }
            }
            // Other instructions can't be constant folded.
            OpCode::MergeInput
            | OpCode::Discard
            | OpCode::Return(_)
            | OpCode::Break
            | OpCode::Continue
            | OpCode::Passthrough
            | OpCode::NextBlock
            | OpCode::Merge(_)
            // OpCode::If and OpCode::LoopIf condition can be Constant(ConstantId), as long as the
            // condition.type_id is TYPE_ID_BOOL. See function
            // validate_if_condition_is_bool(), and test GLSLTestLoops.ForNoCondition.
            | OpCode::If(_)
            | OpCode::LoopIf(_)
            | OpCode::Loop
            | OpCode::DoLoop
            | OpCode::AccessVectorComponent(_, _)
            | OpCode::AccessVectorComponentMulti(_, _)
            | OpCode::AccessVectorComponentDynamic(_, _)
            | OpCode::AccessMatrixColumn(_, _)
            | OpCode::AccessStructField(_, _)
            | OpCode::AccessArrayElement(_, _)
            | OpCode::Load(_)
            | OpCode::Store(_, _)
            | OpCode::Alias(_)
            | OpCode::Call(_, _)
            | OpCode::BuiltIn(_, _)
            | OpCode::Texture(_, _, _) => {}
        };
    }

    fn validate_not_all_args_are_constant(&self, opcode: &OpCode, instruction_args: &[TypedId]) {
        if instruction_args.iter().all(|arg| matches!(arg.id, Id::Constant(_))) {
            self.on_error(format_args!(
                "operation {:?} contains only constant arguments, they should be constant folded",
                opcode
            ));
        }
    }

    fn validate_no_merge_input_opcode_in_block_instruction(&self, opcode: &OpCode) {
        if matches!(opcode, OpCode::MergeInput) {
            self.on_error(format_args!(
                "Invalid Block Instruction {:?}, MergeInput is only allowed in Block input",
                opcode
            ));
        }
    }

    fn validate_all_branch_instructions_have_valid_target(&self) {
        let mut block_meta_data_tracker = Vec::new();
        for entry in &self.ir.function_entries {
            if entry.is_none() {
                // Skip over functions that have been dead-code eliminated.
                continue;
            }
            // Add a default BlockMetaData as the root block's parent BlockMetaData
            block_meta_data_tracker.clear();
            block_meta_data_tracker.push(BlockMetaData::new());
            // call validate_block_branch_instruction_have_valid_target()on root block, it will
            // recursively validate all the child blocks
            self.validate_block_branch_instruction_have_valid_target(
                &mut block_meta_data_tracker,
                entry.as_ref().unwrap(),
            );
        }
    }

    fn validate_block_branch_instruction_have_valid_target(
        &self,
        block_meta_data_tracker: &mut Vec<BlockMetaData>,
        current_block: &Block,
    ) {
        // validate current block's branch instruction
        let block_branch_op_code = current_block.get_terminating_op();
        if !block_branch_op_code.is_branch() {
            self.on_error(format_args!(
                "block does not end with a branch OpCode {:?}",
                block_branch_op_code
            ));
        }

        // if the block branch_op_code requires current block to not contain certain children
        // blocks, validate the children blocks are none
        match block_branch_op_code {
            OpCode::Discard
            | OpCode::Return(_)
            | OpCode::LoopIf(_)
            | OpCode::Merge(_)
            | OpCode::Continue
                if current_block.merge_block.is_some() =>
            {
                self.on_error(format_args!(
                    "block ends with OpCode::Discard, OpCode::Return, OpCode::LoopIf, \
                     OpCode::Merge, OpCode::Continue should not have merge_block"
                ));
            }

            _ => {
                // Other OpCode does have enforce current block to not contain certain children
                // blocks
            }
        }

        // If the block_branch_op_code requires current block to contain certain children blocks,
        // validate the children blocks are present
        match block_branch_op_code {
            OpCode::NextBlock if current_block.merge_block.is_none() => {
                self.on_error(format_args!(
                    "OpCode::NextBlock is missing a valid target. Current block should contain a \
                     merge_block"
                ));
            }
            OpCode::If(_) => {
                if !current_block.has_if_true_block() {
                    self.on_error(format_args!(
                        "OpCode::If is a missing a valid target. Current block should contain a \
                         true block (block1)"
                    ));
                }
                if current_block.merge_block.is_none() {
                    self.on_error(format_args!(
                        "OpCode::If is missing a valid target. Current block should contain a \
                         merge_block"
                    ));
                }
            }
            OpCode::Loop => {
                if current_block.loop_condition.is_none() {
                    self.on_error(format_args!(
                        "OpCode::Loop is a missing a valid target. Current block should contain a \
                         loop_condition block"
                    ));
                }
                if !current_block.has_loop_body_block() {
                    self.on_error(format_args!(
                        "OpCode::Loop is missing a valid target. Current block should contain a \
                         loop body block (block1)"
                    ));
                }
                if current_block.merge_block.is_none() {
                    self.on_error(format_args!(
                        "OpCode::Loop is missing a valid target. Current block should contain a \
                         merge_block"
                    ));
                }
            }
            OpCode::DoLoop => {
                if !current_block.has_loop_body_block() {
                    self.on_error(format_args!(
                        "OpCode::DoLoop is missing a valid target. Current block should contain a \
                         loop body block (block1)"
                    ));
                }
                if current_block.merge_block.is_none() {
                    self.on_error(format_args!(
                        "OpCode::DoLoop is missing a valid target. Current block should contain a \
                         merge_block"
                    ));
                }
            }
            OpCode::Switch(_, case_ids) => {
                if current_block.case_blocks.len() != case_ids.len() {
                    self.on_error(format_args!(
                        "OpCode::Switch case_blocks length mismatches with case_ids length"
                    ));
                }
                if current_block.case_blocks.is_empty() {
                    self.on_error(format_args!(
                        "OpCode::Switch is missing a valid target. Current block should contain \
                         at lease 1 case_block"
                    ));
                }
                if current_block.merge_block.is_none() {
                    self.on_error(format_args!(
                        "OpCode::Switch is missing a valid target. Current block should contain a \
                         merge_block"
                    ));
                }
            }
            _ => {
                // Other OpCode does not enforce the current_block to contain certain children
                // blocks
            }
        }

        // If the block branch_op_code requires its parent block to satisfy certain requirements,
        // validate the requirements.
        let parent_block_meta_data = block_meta_data_tracker.last().unwrap();
        match block_branch_op_code {
            OpCode::Break if !parent_block_meta_data.can_break => {
                self.on_error(format_args!(
                    "OpCode::Break is not within a Loop, DoLoop, or Switch"
                ));
            }
            OpCode::Continue if !parent_block_meta_data.can_continue => {
                self.on_error(format_args!("OpCode::Continue is not within a Loop, DoLoop"));
            }
            OpCode::Passthrough => {
                // First check the Passthrough OpCode is inside a switch case block
                if !parent_block_meta_data.can_passthrough {
                    self.on_error(format_args!(
                        "OpCode::Passthrough not within a Switch Case Block"
                    ));
                }
                // Then check the Passthrough OpCode is not in the last case block
                if parent_block_meta_data.case_block_index + 1
                    >= parent_block_meta_data.switch_case_count
                {
                    self.on_error(format_args!(
                        "OpCode::Passthrough is not allowed on the last Switch Case Block"
                    ));
                }
            }
            OpCode::Merge(_) if !parent_block_meta_data.has_merge_block => {
                self.on_error(format_args!(
                    "OpCode::Merge is missing a valid target. The parent block should contain a \
                     merge block"
                ));
            }
            OpCode::LoopIf(_) if !parent_block_meta_data.is_loop => {
                self.on_error(format_args!(
                    "the block ends with OpCode::LoopIf must be immediate child of the loop that \
                     ends with OpCode::Loop"
                ));
            }
            _ => {
                // Other OpCode does not have enforcement on parent blocks to satitisfy certain
                // conditions
            }
        }

        // Set the BlockMetaData for current_block
        // By default, we inherite the BlockMetaData from the parent
        let mut current_block_meta_data = parent_block_meta_data.clone();

        // Set the BlockMetaData fields that needs to be overwritten by current block
        current_block_meta_data.is_loop =
            matches!(block_branch_op_code, OpCode::Loop | OpCode::DoLoop);
        current_block_meta_data.has_merge_block = current_block.merge_block.is_some();

        // Call validate_block_branch_instruction_have_valid_target() on child blocks.
        // Based on the child block type, the current_block_meta_data field will need different
        // values. We will adjust the current_block_meta_data field values, push
        // current_block_meta_data to the block_meta_data_tracker, and then pop it when we are done
        // with the child block.
        current_block.loop_condition.as_ref().inspect(|loop_condition| {
            let mut current_block_meta_data_for_loop_condition = current_block_meta_data.clone();

            current_block_meta_data_for_loop_condition.can_break = false;
            current_block_meta_data_for_loop_condition.can_continue = false;
            current_block_meta_data_for_loop_condition.can_passthrough = false;
            block_meta_data_tracker.push(current_block_meta_data_for_loop_condition);
            self.validate_block_branch_instruction_have_valid_target(
                block_meta_data_tracker,
                loop_condition,
            );
            block_meta_data_tracker.pop().unwrap();
        });

        current_block.block1.as_ref().inspect(|block1| {
            let mut current_block_meta_data_for_block1 = current_block_meta_data.clone();

            if current_block_meta_data.is_loop {
                current_block_meta_data_for_block1.can_break = true;
                current_block_meta_data_for_block1.can_continue = true;
            }
            block_meta_data_tracker.push(current_block_meta_data_for_block1);
            self.validate_block_branch_instruction_have_valid_target(
                block_meta_data_tracker,
                block1,
            );
            block_meta_data_tracker.pop().unwrap();
        });

        current_block.block2.as_ref().inspect(|block2| {
            let mut current_block_meta_data_for_block2 = current_block_meta_data.clone();

            if current_block_meta_data.is_loop {
                current_block_meta_data_for_block2.can_break = false;
                current_block_meta_data_for_block2.can_continue = true;
            }
            block_meta_data_tracker.push(current_block_meta_data_for_block2);
            self.validate_block_branch_instruction_have_valid_target(
                block_meta_data_tracker,
                block2,
            );
            block_meta_data_tracker.pop().unwrap();
        });

        if let OpCode::Switch(_, _) = block_branch_op_code {
            let mut current_block_meta_data_for_case_block = current_block_meta_data.clone();
            current_block_meta_data_for_case_block.can_break = true;
            current_block_meta_data_for_case_block.can_continue = true;
            current_block_meta_data_for_case_block.can_passthrough = true;
            current_block_meta_data_for_case_block.switch_case_count =
                current_block.case_blocks.len().try_into().unwrap();
            current_block.case_blocks.iter().enumerate().for_each(
                |(case_block_index, case_block)| {
                    current_block_meta_data_for_case_block.case_block_index =
                        case_block_index.try_into().unwrap();
                    block_meta_data_tracker.push(current_block_meta_data_for_case_block.clone());
                    self.validate_block_branch_instruction_have_valid_target(
                        block_meta_data_tracker,
                        case_block,
                    );
                    block_meta_data_tracker.pop().unwrap();
                },
            );
        }

        // For merge_block, it logically is continuation of current_block, we will use
        // current_block's parent BlockMetaData. No need to push current_block_meta_data to
        // the block_meta_data_tracker.
        current_block.merge_block.as_ref().inspect(|merge_block| {
            self.validate_block_branch_instruction_have_valid_target(
                block_meta_data_tracker,
                merge_block,
            );
        });
    }

    fn validate_no_dead_code(&self) {
        traverser::visitor::for_each_function(
            &mut (), // no state to track while traversing all functions
            &self.ir.function_entries,
            |_, _| {}, // do nothing in pre_visit
            |_, block, _, _| {
                self.validate_no_branch_in_the_middle_of_block_instructions(block);
                traverser::visitor::VISIT_SUB_BLOCKS
            }, // validate no branch instruction in the middle of a block during visit
            |_, _| {}, // do nothing in post_visit
        );
    }

    fn validate_no_branch_in_the_middle_of_block_instructions(&self, block: &Block) {
        for instruction in &block.instructions[0..block.instructions.len() - 1] {
            if instruction.is_branch() {
                self.on_error(format_args!(
                    "branch instruction is only allowed in the last instruction in the block"
                ));
            }
        }
    }

    fn validate_merge_block_with_input(&self) {
        traverser::visitor::for_each_function(
            &mut (),
            &self.ir.function_entries,
            |_, _| {}, // do nothing in pre_visit
            |_, block, block_kind, _| {
                self.validate_merge_block_input_prerequisites(block, block_kind);
                traverser::visitor::VISIT_SUB_BLOCKS
            }, /* validate if the current block has a merge_block with input, the input
                        * is set */
            |_, _| {}, // do nothing in post_visit
        );
    }

    fn validate_merge_block_input_prerequisites(
        &self,
        block: &Block,
        block_kind: traverser::BlockKind,
    ) {
        // If the current block has any inputs, the corresponding instruction must be MergeInput
        // OpCode
        block.input.inspect(|input| {
            if !matches!(self.ir.meta.get_instruction(input.id).op, OpCode::MergeInput) {
                self.on_error(format_args!("Block inputs must have MergeInput OpCode"));
            }
        });
        if block.merge_block.as_ref().and_then(|merge_block| merge_block.input).is_some() {
            // merge_block with input is only allowed when current block ends with OpCode::If, and
            // "if true" block exists
            if !matches!(block.get_terminating_op(), OpCode::If(_)) {
                self.on_error(format_args!(
                    "current {:?} block contains a merge block with input, but current block does \
                     not end with OpCode::If",
                    block_kind
                ));
            }
            if block.block1.is_none() {
                self.on_error(format_args!(
                    "current {:?} block contains a merge block with input, at minimum current \
                     block must contains block1 to sets the merge block input value",
                    block_kind
                ));
            }

            // Branch instruction in block1 that jumps to the merge block should have an output
            let block1 = block.block1.as_ref().expect("block1 can't be none");
            let block1_last_op = block1.get_merge_chain_terminating_op();
            if !matches!(block1_last_op, OpCode::Merge(Some(_))) {
                self.on_error(format_args!(
                    "current {:?} block contains a merge block with input, but the branch \
                     instruction in block1 that jumps to the merge block does not have an output",
                    block_kind
                ));
            }

            // Branch instruction in block2 (if block2 exists) that jumps to the merge block should
            // have an output
            if let Some(block2) = block.block2.as_ref() {
                let block2_last_op = block2.get_merge_chain_terminating_op();
                if !matches!(block2_last_op, OpCode::Merge(Some(_))) {
                    self.on_error(format_args!(
                        "current {:?} block contains a merge block with input, but the branch \
                         instruction in block2 that jumps to the merge block does not have an \
                         output",
                        block_kind
                    ));
                }
            }
        }
    }

    fn validate_all_instructions(&self) {
        // All validation that can be done on an instruction in isolation is done in one pass.
        traverser::visitor::for_each_instruction(
            &mut (),
            &self.ir.function_entries,
            &|_, instruction| {
                let (opcode, _result) = instruction.get_op_and_result(&self.ir.meta);
                self.validate_struct_field_in_bounds(opcode);
                self.validate_if_condition_is_bool(opcode);
                self.validate_case_values_are_int_or_uint_constants(opcode);
                self.validate_switch_has_unique_case_values(opcode);
                self.validate_no_constant_foldable_instruction(opcode);
                self.validate_no_merge_input_opcode_in_block_instruction(opcode);
            },
        );
    }

    fn validate_struct_field_in_bounds(&self, opcode: &OpCode) {
        let (struct_type, field_index) = match *opcode {
            OpCode::AccessStructField(struct_id, field_index) => (
                Some(self.ir.meta.get_type(self.ir.meta.get_pointee_type(struct_id.type_id))),
                field_index,
            ),
            OpCode::ExtractStructField(struct_id, field_index) => {
                (Some(self.ir.meta.get_type(struct_id.type_id)), field_index)
            }
            _ => (None, 0),
        };
        if let Some(Type::Struct(_, fields, _)) = struct_type
            && field_index as usize >= fields.len()
        {
            self.on_error(format_args!(
                "OpCode::Access/ExtractStructField on struct {:?} is accessing a field index {} \
                 that is out of bounds",
                struct_type, field_index
            ));
        }
    }

    fn validate_if_condition_is_bool(&self, opcode: &OpCode) {
        match *opcode {
            OpCode::If(condition) | OpCode::LoopIf(condition)
                if condition.type_id != TYPE_ID_BOOL =>
            {
                self.on_error(format_args!(
                    "invalid If/LoopIf instruction: {:?}, consition is not a boolean: {:?}",
                    opcode, condition.type_id
                ));
            }
            _ => (),
        };
    }

    fn validate_case_values_are_int_or_uint_constants(&self, opcode: &OpCode) {
        if let OpCode::Switch(_, case_values) = opcode {
            for case_value_id in case_values.iter().flatten() {
                self.validate_constant_id_is_in_bound_and_alive(
                    *case_value_id,
                    format_args!("switch case values of instruction {:?}", opcode),
                );
                let case_value_constant = &self.ir.meta.all_constants()[case_value_id.id as usize];
                if case_value_constant.type_id != TYPE_ID_INT
                    && case_value_constant.type_id != TYPE_ID_UINT
                {
                    self.on_error(format_args!(
                        "invalid Switch instruction: {:?}, case value is not an integer or \
                         unsigned integer constant: {:?}",
                        opcode, case_value_id
                    ));
                }
            }
        }
    }

    fn validate_switch_has_unique_case_values(&self, opcode: &OpCode) {
        if let OpCode::Switch(_, case_values) = opcode
            && case_values.iter().collect::<HashSet<_>>().len() != case_values.len()
        {
            self.on_error(format_args!(
                "invalid Switch instruction: {:?}, has duplicated case values",
                opcode
            ));
        }
    }
}
