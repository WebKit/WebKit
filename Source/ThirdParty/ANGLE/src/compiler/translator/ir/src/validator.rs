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
//   - Arguments of OpCode that must be pointer type is indeed a pointer:
//     validate_pointer_types_for_operands()
//   - Validate pointer types for instruction operands and results:
//     validate_pointer_types_for_operands(), validate_pointer_types_for_result()
//   - Block inputs are not pointers (not supported by the `astify` pass):
//     validate_merge_block_input_prerequisites()
//
// Decorations:
//   - Between the Smooth, Flat, NoPerspective, Centroid and Sample decorations, they are all
//     mutually exclusive, except NoPerspective can be combined with Centroid or Sample. See
//     ffi::Interpolation in reflection.rs for reference: validate_mutually_exclusive_decorations()
//
// Control flow:
//   - Branches must have the appropriate targets set (merge, trueblock for if, etc); every block
//     ends in branch: validate_all_branch_instructions_have_valid_target()
//   - No branches inside a block (i.e. no dead code): validate_no_dead_code()
//   - For merge blocks that have an input, the branch instruction of blocks that jump to it have an
//     output: validate_merge_block_with_input()
//   - For block that has a merge block with an input, the branch instruction must be If, and the
//     block must contains block1: validate_merge_block_with_input()
//   - Validate that if there's a merge variable in an if block, both true and false blocks exist
//     and they both end in a Merge with an ID.  (Technically, we should be able to also support one
//     block being merge and the other discard/return/break/continue, but no such code can be
//     generated right now): validate_merge_block_with_input()
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
//   - No identity swizzles: validate_no_identity_swizzles()
//   - Precision is not applied to types that are not applicable.  It _is_ applied to types that are
//     applicable (including uniforms and samplers for example).  Needs to work to make sure
//     precision is always assigned: validate_precision(),
//     validate_glsl_result_precision_and_propagation_rules()
//
// Functions:
//   - Check that function parameter variables don't have an initializer:
//     validate_function_parameter_variables()
//   - Check that returned values from functions match the type of the function's return value:
//     validate_function_return_types()

// TODO(http://anglebug.com/349994211): to validate:
//   - If there's a cached "has side effect", that it's correct.
//   - Catch misuse of built-in names.
//   - Loop blocks ends in the appropriate instructions.
//   - Interface variables with NameSource::Internal are unique.
//   - NameSource::Internal names don't start with the user and temporary name prefixes (_u, t and f
//     respectively).
//   - Interface variables with NameSource::ShaderInterface are unique.
//   - NameSource::ShaderInterface and NameSource::Internal are never found inside body
//   - blocks, those should always be Temporary.
//   - Type matches?
//   - Whatever else is in the AST validation currently.
//   - Validate built-ins that accept an out or inout parameter, that the corresponding parameter is
//     passed a Pointer at the call site.  For that matter, do the same for user function calls too.
//   - Check for invalid texture* combinations, like non-shadow samplers with TextureCompare ops, or
//     is_proj is false for cubemaps.
//   - Images with the Rect dimension can only have a Float base type and be 2D samplers (not
//     storage image, array, msaa, etc).
//   - ColumnMajor and RowMajor decorations are mutually exclusive.
//   - ColumnMajor and RowMajor decorations should only apply to matrix types
//   - Instruction result / operand types are correct and consistent. For example:
//     BinaryOpCode::Equal should return a bool type.

use crate::ir::*;
use crate::*;
use std::fmt;

pub fn validate(ir: &IR, previous_operation: &'static str) {
    let validator = Validator::new(ir, previous_operation);
    validator.validate();
}

pub fn validate_glsl_precision_rules(ir: &IR, previous_operation: &'static str) {
    let validator = Validator::new(ir, previous_operation);
    validator.validate_glsl_result_precision_and_propagation_rules();
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

struct PrecisionState<'a> {
    ir_meta: &'a IRMeta,
    function_arg_precisions: HashMap<FunctionId, Vec<Precision>>,
    struct_member_precisions: HashMap<TypeId, Vec<Precision>>,
    current_function_id: FunctionId,
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
    // The name of the operation that was performed before validation was run.  If validation
    // fails, that name makes it easier to blame the transformation that introduced the error.
    operation_before_validate: &'static str,
    max_type_count: u32,
    max_variable_count: u32,
    max_constant_count: u32,
    max_register_count: u32,
}

impl<'a> Validator<'a> {
    // Validator constructor
    fn new(ir: &'a IR, previous_operation: &'static str) -> Validator<'a> {
        Validator {
            ir,
            operation_before_validate: previous_operation,
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
        self.validate_mutually_exclusive_decorations();
        self.validate_no_pointer_to_pointer_type();
        self.validate_all_variables_are_declared_in_scope();
        self.validate_all_registers_are_declared_in_scope();
        self.validate_no_identical_types_with_different_ids();
        self.validate_image_types();
        self.validate_no_dead_code();
        self.validate_all_branch_instructions_have_valid_target();
        self.validate_merge_block_with_input();
        self.validate_all_instructions();
        self.validate_function_parameter_variables();
        self.validate_function_return_types();
        self.validate_precisions();
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

    // Helper function to perform various categories of validation for OpCode instruction TypedId
    // parameters
    //
    // TypedIdValidationCategory::IdInBound: check the id is in bound and alive
    //
    // TypedIdValidationCategory::VariableDeclared: check the variable is declared in scope. Only
    // performed if typed_id.id is Id::Variable type.
    //
    // TypedIdValidationCategory::RegisterDeclared: check the register is declared in scope. Only
    // performed if typed_id.id is Id::Register type.
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
        println!(
            "Internal error: Invalid ANGLE IR after '{}'! {}",
            self.operation_before_validate, validation_error_msg
        );
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

    fn validate_mutually_exclusive_decorations(&self) {
        for variable in
            self.ir.meta.all_variables().iter().filter(|variable| !variable.is_dead_code_eliminated)
        {
            let has_smooth = variable.decorations.has(Decoration::Smooth);
            let has_flat = variable.decorations.has(Decoration::Flat);
            let has_noperspective = variable.decorations.has(Decoration::NoPerspective);

            let has_centroid = variable.decorations.has(Decoration::Centroid);
            let has_sample = variable.decorations.has(Decoration::Sample);

            if has_smooth && (has_flat || has_noperspective || has_centroid || has_sample) {
                self.on_error(format_args!(
                    "invalid variable: {:?}, Smooth decoration is mutually exclusive with other \
                     interpolation decorations",
                    variable
                ));
            } else if has_flat && (has_noperspective || has_centroid || has_sample) {
                self.on_error(format_args!(
                    "invalid variable: {:?}, Flat decoration is mutually exclusive with other \
                     interpolation decorations",
                    variable
                ));
            } else if has_centroid && has_sample {
                self.on_error(format_args!(
                    "invalid variable: {:?}, Centroid and Sample decorations are mutually \
                     exclusive",
                    variable
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
        block.input.inspect(|input| {
            // If the current block has any inputs, the corresponding instruction must be MergeInput
            // OpCode
            if !matches!(self.ir.meta.get_instruction(input.id).op, OpCode::MergeInput) {
                self.on_error(format_args!("Block inputs must have MergeInput OpCode"));
            }
            // Block input must not be pointer, astify uses this input register as the result of
            // generated Load instruction
            if self.ir.meta.get_type(input.type_id).is_pointer() {
                self.on_error(format_args!(
                    "invalid {:?} block, input {:?} must not be a pointer",
                    block_kind, input
                ));
            }
        });
        if block.merge_block.as_ref().and_then(|merge_block| merge_block.input).is_some() {
            // merge_block with input is only allowed when current block ends with OpCode::If, and
            // both the "true" and "false" blocks exist
            if !matches!(block.get_terminating_op(), OpCode::If(_)) {
                self.on_error(format_args!(
                    "current {:?} block contains a merge block with input, but current block does \
                     not end with OpCode::If",
                    block_kind
                ));
            }
            if block.block1.is_none() || block.block2.is_none() {
                self.on_error(format_args!(
                    "current {:?} block contains a merge block with input, current block must \
                     contain both block1 and block2 to set the merge block input value",
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

            // Branch instruction in block2 that jumps to the merge block should have an output
            let block2 = block.block2.as_ref().expect("block2 can't be none");
            let block2_last_op = block2.get_merge_chain_terminating_op();
            if !matches!(block2_last_op, OpCode::Merge(Some(_))) {
                self.on_error(format_args!(
                    "current {:?} block contains a merge block with input, but the branch \
                     instruction in block2 that jumps to the merge block does not have an output",
                    block_kind
                ));
            }
        }
    }

    fn validate_function_parameter_variables(&self) {
        traverser::visitor::for_each_function(
            &mut (),
            &self.ir.function_entries,
            |_, function_id| {
                let function = self.ir.meta.get_function(function_id);
                for param in &function.params {
                    let param_variable = self.ir.meta.get_variable(param.variable_id);
                    if param_variable.initializer.is_some() {
                        self.on_error(format_args!(
                            "invalid function {:?}, parameter variable {:?} {:?} must not have \
                             initializer",
                            function_id, param.variable_id, param_variable
                        ));
                    }
                }
            },
            |_, _, _, _| traverser::visitor::STOP,
            |_, _| {}, // do nothing in post_visit
        );
    }

    fn validate_function_return_types(&self) {
        let mut current_func_id = FunctionId { id: 0 };
        traverser::visitor::for_each_function(
            &mut current_func_id,
            &self.ir.function_entries,
            |current_func_id, function_id| {
                *current_func_id = function_id;
            },
            |current_func_id, block, _, _| {
                let function = self.ir.meta.get_function(*current_func_id);
                if let &OpCode::Return(return_value) = block.get_terminating_op() {
                    match return_value {
                        Some(return_value) => {
                            if return_value.type_id != function.return_type_id {
                                self.on_error(format_args!(
                                    "invalid return value {:?} from function {:?}, expected type \
                                     {:?}",
                                    return_value, *current_func_id, function.return_type_id
                                ));
                            }
                        }
                        None => {
                            if function.return_type_id != TYPE_ID_VOID {
                                self.on_error(format_args!(
                                    "invalid return with no value from function {:?}, expected \
                                     type {:?}",
                                    *current_func_id, function.return_type_id
                                ));
                            }
                        }
                    }
                }
                traverser::visitor::VISIT_SUB_BLOCKS
            },
            |_, _| {}, // do nothing in post_visit
        );
    }

    fn validate_all_instructions(&self) {
        // All validation that can be done on an instruction in isolation is done in one pass.
        traverser::visitor::for_each_instruction(
            &mut (),
            &self.ir.function_entries,
            &|_, instruction| {
                let (opcode, result) = instruction.get_op_and_result(&self.ir.meta);
                self.validate_struct_field_in_bounds(opcode);
                self.validate_if_condition_is_bool(opcode);
                self.validate_case_values_are_int_or_uint_constants(opcode);
                self.validate_switch_has_unique_case_values(opcode);
                self.validate_no_constant_foldable_instruction(opcode);
                self.validate_no_merge_input_opcode_in_block_instruction(opcode);
                self.validate_pointer_types_for_operands(opcode);
                self.validate_pointer_types_for_result(opcode, result);
                self.validate_no_identity_swizzles(opcode);
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

    fn validate_pointer_types_for_result(&self, opcode: &OpCode, result: Option<TypedRegisterId>) {
        let Some(result) = result else {
            return;
        };

        let is_result_ptr = self.ir.meta.get_type(result.type_id).is_pointer();
        match opcode {
            // Instructions that produce pointer results
            &OpCode::AccessStructField(..)
            | &OpCode::AccessVectorComponent(..)
            | &OpCode::AccessVectorComponentMulti(..)
            | &OpCode::AccessMatrixColumn(..)
            | &OpCode::AccessArrayElement(..)
            | &OpCode::AccessVectorComponentDynamic(..) => {
                if !is_result_ptr {
                    self.on_error(format_args!(
                        "invalid instruction: {:?}, result register {:?} must be a pointer",
                        opcode, result
                    ));
                }
            }
            // Instructions that produce value results
            &OpCode::ExtractVectorComponent(..)
            | &OpCode::ExtractVectorComponentMulti(..)
            | &OpCode::ExtractStructField(..)
            | &OpCode::ExtractVectorComponentDynamic(..)
            | &OpCode::ExtractMatrixColumn(..)
            | &OpCode::ExtractArrayElement(..)
            | &OpCode::ConstructScalarFromScalar(..)
            | &OpCode::ConstructVectorFromScalar(..)
            | &OpCode::ConstructMatrixFromScalar(..)
            | &OpCode::ConstructMatrixFromMatrix(..)
            | &OpCode::ConstructVectorFromMultiple(..)
            | &OpCode::ConstructMatrixFromMultiple(..)
            | &OpCode::ConstructStruct(..)
            | &OpCode::ConstructArray(..)
            | &OpCode::Load(..)
            | &OpCode::Call(..)
            | &OpCode::Unary(..)
            | &OpCode::Binary(..)
            | &OpCode::BuiltIn(..)
            | &OpCode::Texture(..) => {
                if is_result_ptr {
                    self.on_error(format_args!(
                        "invalid instruction: {:?}, result register {:?} must not be a pointer",
                        opcode, result
                    ));
                }
            }
            // Alias must match the pointer/non-pointer type of the old id.
            &OpCode::Alias(old_id) => {
                let is_old_id_ptr = self.ir.meta.get_type(old_id.type_id).is_pointer();
                if is_result_ptr != is_old_id_ptr {
                    self.on_error(format_args!(
                        "invalid instruction: {:?}, result register {:?} must match \
                         pointer/non-pointer type of the old id {:?}",
                        opcode, result, old_id
                    ));
                }
            }
            // Instructions that produce no result
            &OpCode::MergeInput
            | &OpCode::Discard
            | &OpCode::Return(..)
            | &OpCode::Break
            | &OpCode::Continue
            | &OpCode::Passthrough
            | &OpCode::NextBlock
            | &OpCode::Merge(..)
            | &OpCode::If(..)
            | &OpCode::Loop
            | &OpCode::DoLoop
            | &OpCode::LoopIf(..)
            | &OpCode::Switch(..)
            | &OpCode::Store(..) => {}
        }
    }

    fn validate_pointer_types_for_operands(&self, opcode: &OpCode) {
        let validate_operand_is_not_pointer = |operand: TypedId| {
            if self.ir.meta.get_type(operand.type_id).is_pointer() {
                self.on_error(format_args!(
                    "invalid instruction: {:?}, operand {:?} must not be a pointer",
                    opcode, operand
                ));
            }
        };
        let validate_operand_is_pointer = |operand: TypedId| {
            if !self.ir.meta.get_type(operand.type_id).is_pointer() {
                self.on_error(format_args!(
                    "invalid instruction: {:?}, operand {:?} must be a pointer",
                    opcode, operand
                ));
            }
        };
        match opcode {
            // Single pointer operand
            &OpCode::Unary(UnaryOpCode::ArrayLength, ptr)
            | &OpCode::Unary(UnaryOpCode::PrefixIncrement, ptr)
            | &OpCode::Unary(UnaryOpCode::PrefixDecrement, ptr)
            | &OpCode::Unary(UnaryOpCode::PostfixIncrement, ptr)
            | &OpCode::Unary(UnaryOpCode::PostfixDecrement, ptr)
            | &OpCode::Unary(UnaryOpCode::AtomicCounter, ptr)
            | &OpCode::Unary(UnaryOpCode::AtomicCounterIncrement, ptr)
            | &OpCode::Unary(UnaryOpCode::AtomicCounterDecrement, ptr)
            | &OpCode::AccessStructField(ptr, _)
            | &OpCode::AccessVectorComponent(ptr, _)
            | &OpCode::AccessVectorComponentMulti(ptr, _)
            | &OpCode::Load(ptr) => {
                validate_operand_is_pointer(ptr);
            }
            // Single value operand
            &OpCode::Return(Some(val))
            | &OpCode::Merge(Some(val))
            | &OpCode::If(val)
            | &OpCode::LoopIf(val)
            | &OpCode::Switch(val, _)
            | &OpCode::ExtractVectorComponent(val, _)
            | &OpCode::ExtractVectorComponentMulti(val, _)
            | &OpCode::ExtractStructField(val, _)
            | &OpCode::ConstructScalarFromScalar(val)
            | &OpCode::ConstructVectorFromScalar(val)
            | &OpCode::ConstructMatrixFromScalar(val)
            | &OpCode::ConstructMatrixFromMatrix(val)
            | &OpCode::Unary(_, val) => {
                validate_operand_is_not_pointer(val);
            }
            // Pointer operand followed by value operand.
            &OpCode::AccessMatrixColumn(ptr, val)
            | &OpCode::AccessArrayElement(ptr, val)
            | &OpCode::AccessVectorComponentDynamic(ptr, val)
            | &OpCode::Store(ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicAdd, ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicMin, ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicMax, ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicAnd, ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicOr, ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicXor, ptr, val)
            | &OpCode::Binary(BinaryOpCode::AtomicExchange, ptr, val) => {
                validate_operand_is_pointer(ptr);
                validate_operand_is_not_pointer(val);
            }
            // Value operand followed by pointer operand.
            &OpCode::Binary(BinaryOpCode::Modf, val, ptr)
            | &OpCode::Binary(BinaryOpCode::Frexp, val, ptr) => {
                validate_operand_is_not_pointer(val);
                validate_operand_is_pointer(ptr);
            }
            // Two value operands
            &OpCode::ExtractVectorComponentDynamic(val1, val2)
            | &OpCode::ExtractMatrixColumn(val1, val2)
            | &OpCode::ExtractArrayElement(val1, val2)
            | &OpCode::Binary(_, val1, val2) => {
                validate_operand_is_not_pointer(val1);
                validate_operand_is_not_pointer(val2);
            }

            // First operand is a pointer, other operands are values.
            &OpCode::BuiltIn(BuiltInOpCode::AtomicCompSwap, ref args) => {
                for (i, &arg) in args.iter().enumerate() {
                    if i == 0 {
                        validate_operand_is_pointer(arg);
                    } else {
                        validate_operand_is_not_pointer(arg);
                    }
                }
            }
            // Third operand is a pointer, other operands are values.
            &OpCode::BuiltIn(BuiltInOpCode::UaddCarry, ref args)
            | &OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, ref args) => {
                for (i, &arg) in args.iter().enumerate() {
                    if i == 2 {
                        validate_operand_is_pointer(arg);
                    } else {
                        validate_operand_is_not_pointer(arg);
                    }
                }
            }
            // Third and fourth operands are pointers, other operands are values.
            &OpCode::BuiltIn(BuiltInOpCode::UmulExtended, ref args)
            | &OpCode::BuiltIn(BuiltInOpCode::ImulExtended, ref args) => {
                for (i, &arg) in args.iter().enumerate() {
                    if i == 2 || i == 3 {
                        validate_operand_is_pointer(arg);
                    } else {
                        validate_operand_is_not_pointer(arg);
                    }
                }
            }
            // All operands are values.
            &OpCode::ConstructVectorFromMultiple(ref vals)
            | &OpCode::ConstructMatrixFromMultiple(ref vals)
            | &OpCode::ConstructStruct(ref vals)
            | &OpCode::ConstructArray(ref vals)
            | &OpCode::BuiltIn(_, ref vals) => {
                for &val in vals {
                    validate_operand_is_not_pointer(val);
                }
            }

            &OpCode::Call(function_id, ref args) => {
                let param_directions = self
                    .ir
                    .meta
                    .get_function(function_id)
                    .params
                    .iter()
                    .map(|param| param.direction);
                args.iter().zip(param_directions).for_each(|(&arg, direction)| {
                    match direction {
                        FunctionParamDirection::InputOutput | FunctionParamDirection::Output => {
                            // `out` and `in out` parameters are always pointers.
                            validate_operand_is_pointer(arg);
                        }
                        FunctionParamDirection::Input => {
                            // `in` parameter could be pointer or value.
                        }
                    };
                });
            }

            // Texture instructions take value operands only.
            &OpCode::Texture(ref texture_opcode, sampler, coord) => {
                validate_operand_is_not_pointer(sampler);
                validate_operand_is_not_pointer(coord);
                match *texture_opcode {
                    TextureOpCode::Implicit { offset, .. } | TextureOpCode::Gather { offset } => {
                        if let Some(offset) = offset {
                            validate_operand_is_not_pointer(offset);
                        }
                    }
                    TextureOpCode::Compare { compare } => {
                        validate_operand_is_not_pointer(compare);
                    }
                    TextureOpCode::Lod { lod, offset, .. } => {
                        validate_operand_is_not_pointer(lod);
                        if let Some(offset) = offset {
                            validate_operand_is_not_pointer(offset);
                        }
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        validate_operand_is_not_pointer(compare);
                        validate_operand_is_not_pointer(lod);
                    }
                    TextureOpCode::Bias { bias, offset, .. } => {
                        validate_operand_is_not_pointer(bias);
                        if let Some(offset) = offset {
                            validate_operand_is_not_pointer(offset);
                        }
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        validate_operand_is_not_pointer(compare);
                        validate_operand_is_not_pointer(bias);
                    }
                    TextureOpCode::Grad { dx, dy, offset, .. } => {
                        validate_operand_is_not_pointer(dx);
                        validate_operand_is_not_pointer(dy);
                        if let Some(offset) = offset {
                            validate_operand_is_not_pointer(offset);
                        }
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        validate_operand_is_not_pointer(component);
                        if let Some(offset) = offset {
                            validate_operand_is_not_pointer(offset);
                        }
                    }
                    TextureOpCode::GatherRef { refz, offset } => {
                        validate_operand_is_not_pointer(refz);
                        if let Some(offset) = offset {
                            validate_operand_is_not_pointer(offset);
                        }
                    }
                }
            }
            _ => (),
        }
    }

    fn validate_no_identity_swizzles(&self, opcode: &OpCode) {
        let (vec_type, components) = match *opcode {
            OpCode::ExtractVectorComponentMulti(vector, ref components) => {
                (self.ir.meta.get_type(vector.type_id), components)
            }
            OpCode::AccessVectorComponentMulti(vector_ptr, ref components) => {
                // Access* takes a pointer to vector
                let pointee_type_id = self.ir.meta.get_pointee_type(vector_ptr.type_id);
                (self.ir.meta.get_type(pointee_type_id), components)
            }
            _ => {
                return;
            }
        };
        let vec_size = vec_type.get_vector_size().unwrap() as usize;
        if components.len() != vec_size {
            return;
        }
        for (index, &component) in components.iter().enumerate() {
            if component != index as u32 {
                return;
            }
        }
        // Every component is selected in original order
        self.on_error(format_args!(
            "invalid instruction: {:?}, identity swizzles found, components selected: {:?}",
            opcode, components
        ));
    }

    fn validate_precisions(&self) {
        let mut precision_state = PrecisionState {
            ir_meta: &self.ir.meta,
            function_arg_precisions: HashMap::new(),
            struct_member_precisions: HashMap::new(),
            current_function_id: FunctionId { id: 0 },
        };

        // Prepare a map of structs to the precision of their members.
        precision_state.ir_meta.all_types().iter().enumerate().for_each(|(index, type_info)| {
            if let Type::Struct(_, fields, _) = type_info {
                let member_precision = fields.iter().map(|field| field.precision).collect();
                precision_state
                    .struct_member_precisions
                    .insert(TypeId { id: index as u32 }, member_precision);
            }
        });

        traverser::visitor::for_each_function(
            &mut precision_state,
            &self.ir.function_entries,
            // PreVisit: do nothing
            |_, _| {},
            // BlockVisit: check the precisions in instructions in entry block
            // Return VISIT_SUB_BLOCKS so the for loop below is also executed for sub-blocks
            |precision_state, entry, _block_kind, _usize| {
                for instruction in &entry.instructions {
                    let (opcode, result) = instruction.get_op_and_result(&self.ir.meta);
                    self.validate_instruction_precision(precision_state, opcode, result);
                }
                traverser::visitor::VISIT_SUB_BLOCKS
            },
            // PostVisit: do nothing
            |_, _| {},
        );
    }

    fn validate_instruction_precision(
        &self,
        precision_state: &PrecisionState,
        opcode: &OpCode,
        result: Option<TypedRegisterId>,
    ) {
        // We can't ensure when the register is used, its' precision is the same as the
        // precision when it is produced:
        // In the following example:
        //   r19  (t10) =   Load v10                           [highp]
        //   r20  (t32) =   Load v4                            [mediump]
        //   r10   (t9) =   ImageLoad (r20, r19)               [mediump]
        //   r21  (t10) =   Load v10                           [highp]
        //   r22  (t32) =   Load v3                            [lowp]
        //                  MemoryBarrierImage ()
        //                  ImageStore (r22, r21, r10)
        //                  Return
        // when r10 is produced, it is set with mediump, because r20 and v4 are mediump
        // when r10 is used in ImageStore instruction, it is used with lowp, because the
        // first operand r22 is lowp.

        // We can't force variable precision used in instruction to match with the
        // variable declared precision For Example, given the following shader code:
        //   layout(binding=0, rgba8) uniform lowp pixelLocalANGLE dst;
        //   layout(binding=1, rgba8) uniform mediump pixelLocalANGLE src1;
        //   void store(highp pixelLocalANGLE d, lowp pixelLocalANGLE s) {
        //       pixelLocalStoreANGLE(d, pixelLocalLoadANGLE(s));
        //   }
        //   void main()
        //   {
        //       store(dst, src1);
        //   }
        // src1 is declared as mediump, but function store() takes src1 as lowp

        // For TypedId operand, verify the Precision::NotApplicable is only used for the types that
        // are not applicable for precision
        let validate_typedid_operand_precision_applicability = |typed_id: &TypedId| {
            let is_precision_applicable =
                util::is_precision_applicable_to_type(&self.ir.meta, typed_id.type_id);
            if !is_precision_applicable && typed_id.precision != Precision::NotApplicable {
                self.on_error(format_args!(
                    "Invalid instruction {:?}, operand {:?} has a precision, but precision is not \
                     applicable to this type",
                    opcode, typed_id
                ));
            }

            if is_precision_applicable && typed_id.precision == Precision::NotApplicable {
                self.on_error(format_args!(
                    "Invalid instruction {:?}, operand {:?} has a precision applicable base type, \
                     but precision is NotApplicable",
                    opcode, typed_id
                ));
            }
        };

        // For result, verify the Precision::NotApplicable is only used for the types that are not
        // applicable for precision
        if let Some(result) = result {
            let is_precision_applicable =
                util::is_precision_applicable_to_type(&self.ir.meta, result.type_id);
            if !is_precision_applicable && result.precision != Precision::NotApplicable {
                self.on_error(format_args!(
                    "Invalid instruction {:?}, result {:?} has a precision, but precision is not \
                     applicable to this type",
                    opcode, result,
                ));
            }

            if is_precision_applicable && result.precision == Precision::NotApplicable {
                self.on_error(format_args!(
                    "Invalid instruction {:?}, result {:?} has a precision applicable base type, \
                     but precision is NotApplicable",
                    opcode, result
                ));
            }
        }

        match opcode {
            // Not possible. See validate_no_merge_input_opcode_in_block_instruction().
            OpCode::MergeInput => {}
            // OpCode does not take any TypedId parameters, nor return any result. No precision
            // validation is required.
            OpCode::Discard
            | OpCode::Break
            | OpCode::Continue
            | OpCode::Passthrough
            | OpCode::NextBlock
            | OpCode::Loop
            | OpCode::DoLoop
            | OpCode::Return(None)
            | OpCode::Merge(None) => {}
            // OpCode that takes one TypedId operand but not return any result
            OpCode::Return(Some(id))
            | OpCode::Merge(Some(id))
            | OpCode::If(id)
            | OpCode::LoopIf(id)
            | OpCode::Switch(id, _) => {
                validate_typedid_operand_precision_applicability(id);
            }
            // OpCode that takes two TypedId operands but not return any result
            OpCode::Store(ptr_id, val_id) => {
                validate_typedid_operand_precision_applicability(ptr_id);
                validate_typedid_operand_precision_applicability(val_id);
            }
            // OpCode that takes a list of TypedId operands but not return any result
            OpCode::Call(_function_id, args) => {
                for arg in args {
                    validate_typedid_operand_precision_applicability(arg);
                }
            }
            // Struct Access and Struct Extract OpCode
            // Result precision should match with the precision of the struct member being indexed
            OpCode::AccessStructField(struct_id, field_index)
            | OpCode::ExtractStructField(struct_id, field_index) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                let mut struct_type_id = struct_id.type_id;
                if self.ir.meta.get_type(struct_type_id).is_pointer() {
                    struct_type_id = self.ir.meta.get_pointee_type(struct_type_id);
                }
                debug_assert!(self.ir.meta.get_type(struct_type_id).is_struct());
                debug_assert!(
                    precision_state.struct_member_precisions.contains_key(&struct_type_id)
                );
                debug_assert!(
                    precision_state.struct_member_precisions[&struct_type_id].len()
                        > *field_index as usize
                );
                if result.precision
                    != precision_state.struct_member_precisions[&struct_type_id]
                        [*field_index as usize]
                {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}, result {:?} precision does not match with \
                         struct field {:?} precision",
                        opcode, result, field_index
                    ));
                }
            }
            // OpCode that takes one TypedId operand and returns a result
            OpCode::Load(id)
            | OpCode::Alias(id)
            | OpCode::ConstructScalarFromScalar(id)
            | OpCode::ConstructVectorFromScalar(id)
            | OpCode::ConstructMatrixFromScalar(id)
            | OpCode::ConstructMatrixFromMatrix(id)
            | OpCode::Unary(UnaryOpCode::Negate, id)
            | OpCode::Unary(UnaryOpCode::BitwiseNot, id)
            | OpCode::Unary(UnaryOpCode::PrefixIncrement, id)
            | OpCode::Unary(UnaryOpCode::PrefixDecrement, id)
            | OpCode::Unary(UnaryOpCode::PostfixIncrement, id)
            | OpCode::Unary(UnaryOpCode::PostfixDecrement, id)
            | OpCode::Unary(UnaryOpCode::Radians, id)
            | OpCode::Unary(UnaryOpCode::Degrees, id)
            | OpCode::Unary(UnaryOpCode::Sin, id)
            | OpCode::Unary(UnaryOpCode::Cos, id)
            | OpCode::Unary(UnaryOpCode::Tan, id)
            | OpCode::Unary(UnaryOpCode::Asin, id)
            | OpCode::Unary(UnaryOpCode::Acos, id)
            | OpCode::Unary(UnaryOpCode::Atan, id)
            | OpCode::Unary(UnaryOpCode::Sinh, id)
            | OpCode::Unary(UnaryOpCode::Cosh, id)
            | OpCode::Unary(UnaryOpCode::Tanh, id)
            | OpCode::Unary(UnaryOpCode::Asinh, id)
            | OpCode::Unary(UnaryOpCode::Acosh, id)
            | OpCode::Unary(UnaryOpCode::Atanh, id)
            | OpCode::Unary(UnaryOpCode::Exp, id)
            | OpCode::Unary(UnaryOpCode::Log, id)
            | OpCode::Unary(UnaryOpCode::Exp2, id)
            | OpCode::Unary(UnaryOpCode::Log2, id)
            | OpCode::Unary(UnaryOpCode::Sqrt, id)
            | OpCode::Unary(UnaryOpCode::Inversesqrt, id)
            | OpCode::Unary(UnaryOpCode::Abs, id)
            | OpCode::Unary(UnaryOpCode::Sign, id)
            | OpCode::Unary(UnaryOpCode::Floor, id)
            | OpCode::Unary(UnaryOpCode::Trunc, id)
            | OpCode::Unary(UnaryOpCode::Round, id)
            | OpCode::Unary(UnaryOpCode::RoundEven, id)
            | OpCode::Unary(UnaryOpCode::Ceil, id)
            | OpCode::Unary(UnaryOpCode::Fract, id)
            | OpCode::Unary(UnaryOpCode::Length, id)
            | OpCode::Unary(UnaryOpCode::Normalize, id)
            | OpCode::Unary(UnaryOpCode::Transpose, id)
            | OpCode::Unary(UnaryOpCode::Determinant, id)
            | OpCode::Unary(UnaryOpCode::Inverse, id)
            | OpCode::Unary(UnaryOpCode::DFdx, id)
            | OpCode::Unary(UnaryOpCode::DFdy, id)
            | OpCode::Unary(UnaryOpCode::Fwidth, id)
            | OpCode::Unary(UnaryOpCode::InterpolateAtCentroid, id)
            | OpCode::Unary(UnaryOpCode::PixelLocalLoadANGLE, id) => {
                validate_typedid_operand_precision_applicability(id);
            }
            // OpCode that takes two TypedId operands and returns a result
            OpCode::Binary(BinaryOpCode::Add, id1, id2)
            | OpCode::Binary(BinaryOpCode::Sub, id1, id2)
            | OpCode::Binary(BinaryOpCode::Mul, id1, id2)
            | OpCode::Binary(BinaryOpCode::VectorTimesScalar, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixTimesScalar, id1, id2)
            | OpCode::Binary(BinaryOpCode::VectorTimesMatrix, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixTimesVector, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixTimesMatrix, id1, id2)
            | OpCode::Binary(BinaryOpCode::Div, id1, id2)
            | OpCode::Binary(BinaryOpCode::IMod, id1, id2)
            | OpCode::Binary(BinaryOpCode::Atan, id1, id2)
            | OpCode::Binary(BinaryOpCode::Pow, id1, id2)
            | OpCode::Binary(BinaryOpCode::Mod, id1, id2)
            | OpCode::Binary(BinaryOpCode::Min, id1, id2)
            | OpCode::Binary(BinaryOpCode::Max, id1, id2)
            | OpCode::Binary(BinaryOpCode::Step, id1, id2)
            | OpCode::Binary(BinaryOpCode::Modf, id1, id2)
            | OpCode::Binary(BinaryOpCode::Distance, id1, id2)
            | OpCode::Binary(BinaryOpCode::Dot, id1, id2)
            | OpCode::Binary(BinaryOpCode::Cross, id1, id2)
            | OpCode::Binary(BinaryOpCode::Reflect, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixCompMult, id1, id2)
            | OpCode::Binary(BinaryOpCode::OuterProduct, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitwiseOr, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitwiseXor, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitwiseAnd, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitShiftLeft, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitShiftRight, id1, id2)
            | OpCode::Binary(BinaryOpCode::InterpolateAtSample, id1, id2)
            | OpCode::Binary(BinaryOpCode::InterpolateAtOffset, id1, id2) => {
                validate_typedid_operand_precision_applicability(id1);
                validate_typedid_operand_precision_applicability(id2);
            }
            // OpCode that takes a list of TypedId operands and returns a result
            OpCode::ConstructVectorFromMultiple(args)
            | OpCode::ConstructMatrixFromMultiple(args)
            | OpCode::ConstructArray(args)
            | OpCode::ConstructStruct(args)
            | OpCode::BuiltIn(BuiltInOpCode::BitfieldExtract, args)
            | OpCode::BuiltIn(BuiltInOpCode::BitfieldInsert, args)
            | OpCode::BuiltIn(BuiltInOpCode::InterpolateAtCenter, args)
            | OpCode::BuiltIn(BuiltInOpCode::TextureQueryLod, args)
            | OpCode::BuiltIn(BuiltInOpCode::TexelFetch, args)
            | OpCode::BuiltIn(BuiltInOpCode::TexelFetchOffset, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageLoad, args)
            | OpCode::BuiltIn(BuiltInOpCode::SubpassLoad, args)
            | OpCode::BuiltIn(BuiltInOpCode::Clamp, args)
            | OpCode::BuiltIn(BuiltInOpCode::Mix, args)
            | OpCode::BuiltIn(BuiltInOpCode::Smoothstep, args)
            | OpCode::BuiltIn(BuiltInOpCode::Fma, args)
            | OpCode::BuiltIn(BuiltInOpCode::Faceforward, args)
            | OpCode::BuiltIn(BuiltInOpCode::Refract, args)
            | OpCode::BuiltIn(BuiltInOpCode::Rgb2Yuv, args)
            | OpCode::BuiltIn(BuiltInOpCode::Yuv2Rgb, args)
            | OpCode::BuiltIn(BuiltInOpCode::Saturate, args)
            | OpCode::BuiltIn(BuiltInOpCode::PixelLocalStoreANGLE, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageStore, args)
            | OpCode::BuiltIn(BuiltInOpCode::UmulExtended, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImulExtended, args)
            | OpCode::BuiltIn(BuiltInOpCode::MemoryBarrier, args)
            | OpCode::BuiltIn(BuiltInOpCode::MemoryBarrierAtomicCounter, args)
            | OpCode::BuiltIn(BuiltInOpCode::MemoryBarrierBuffer, args)
            | OpCode::BuiltIn(BuiltInOpCode::MemoryBarrierImage, args)
            | OpCode::BuiltIn(BuiltInOpCode::Barrier, args)
            | OpCode::BuiltIn(BuiltInOpCode::MemoryBarrierShared, args)
            | OpCode::BuiltIn(BuiltInOpCode::GroupMemoryBarrier, args)
            | OpCode::BuiltIn(BuiltInOpCode::EmitVertex, args)
            | OpCode::BuiltIn(BuiltInOpCode::EndPrimitive, args)
            | OpCode::BuiltIn(BuiltInOpCode::BeginInvocationInterlockNV, args)
            | OpCode::BuiltIn(BuiltInOpCode::EndInvocationInterlockNV, args)
            | OpCode::BuiltIn(BuiltInOpCode::BeginFragmentShaderOrderingINTEL, args)
            | OpCode::BuiltIn(BuiltInOpCode::BeginInvocationInterlockARB, args)
            | OpCode::BuiltIn(BuiltInOpCode::EndInvocationInterlockARB, args)
            | OpCode::BuiltIn(BuiltInOpCode::LoopForwardProgress, args) => {
                for arg in args {
                    validate_typedid_operand_precision_applicability(arg);
                }
            }
            // Extract and Access OpCode that takes one TypedId operand and return a result,
            // and result precision should match with the first operand
            OpCode::ExtractVectorComponent(id, _)
            | OpCode::ExtractVectorComponentMulti(id, _)
            | OpCode::AccessVectorComponent(id, _)
            | OpCode::AccessVectorComponentMulti(id, _) => {
                validate_typedid_operand_precision_applicability(id);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id.precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the operand {:?}",
                        opcode, result, id
                    ));
                }
            }
            // Extract and Access OpCode that takes two TypedId operands and return a result, and
            // result precision should match with the first operand
            OpCode::ExtractVectorComponentDynamic(id1, id2)
            | OpCode::ExtractMatrixColumn(id1, id2)
            | OpCode::ExtractArrayElement(id1, id2)
            | OpCode::AccessVectorComponentDynamic(id1, id2)
            | OpCode::AccessMatrixColumn(id1, id2)
            | OpCode::AccessArrayElement(id1, id2) => {
                validate_typedid_operand_precision_applicability(id1);
                validate_typedid_operand_precision_applicability(id2);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id1.precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the first operand {:?}",
                        opcode, result, id1
                    ));
                }
            }
            // Unary OpCode, result precision must be High
            OpCode::Unary(UnaryOpCode::ArrayLength, id)
            | OpCode::Unary(UnaryOpCode::PackSnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::PackHalf2x16, id)
            | OpCode::Unary(UnaryOpCode::PackUnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::PackUnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::PackSnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::FloatBitsToInt, id)
            | OpCode::Unary(UnaryOpCode::FloatBitsToUint, id)
            | OpCode::Unary(UnaryOpCode::IntBitsToFloat, id)
            | OpCode::Unary(UnaryOpCode::UintBitsToFloat, id)
            | OpCode::Unary(UnaryOpCode::UnpackSnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::UnpackUnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::BitfieldReverse, id)
            | OpCode::Unary(UnaryOpCode::AtomicCounter, id)
            | OpCode::Unary(UnaryOpCode::AtomicCounterIncrement, id)
            | OpCode::Unary(UnaryOpCode::AtomicCounterDecrement, id)
            | OpCode::Unary(UnaryOpCode::ImageSize, id) => {
                validate_typedid_operand_precision_applicability(id);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::High {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision must be High, but the \
                         actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // Unary OpCode, result precision must be Medium
            OpCode::Unary(UnaryOpCode::UnpackHalf2x16, id)
            | OpCode::Unary(UnaryOpCode::UnpackUnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::UnpackSnorm4x8, id) => {
                validate_typedid_operand_precision_applicability(id);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::Medium {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision must be Medium, but the \
                         actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // Unary OpCode, result precision must be Low
            OpCode::Unary(UnaryOpCode::BitCount, id)
            | OpCode::Unary(UnaryOpCode::FindLSB, id)
            | OpCode::Unary(UnaryOpCode::FindMSB, id) => {
                validate_typedid_operand_precision_applicability(id);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::Low {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision must be Low, but the \
                         actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // Unary OpCode, result precision must be NotApplicable
            OpCode::Unary(UnaryOpCode::Isnan, id)
            | OpCode::Unary(UnaryOpCode::Isinf, id)
            | OpCode::Unary(UnaryOpCode::LogicalNot, id)
            | OpCode::Unary(UnaryOpCode::Any, id)
            | OpCode::Unary(UnaryOpCode::All, id)
            | OpCode::Unary(UnaryOpCode::Not, id) => {
                validate_typedid_operand_precision_applicability(id);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::NotApplicable {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision must be NotApplicable, \
                         but the actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // Binary OpCode, result precision must be High
            OpCode::Binary(BinaryOpCode::Frexp, id1, id2)
            | OpCode::Binary(BinaryOpCode::Ldexp, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicAdd, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicMin, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicMax, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicAnd, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicOr, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicXor, id1, id2)
            | OpCode::Binary(BinaryOpCode::AtomicExchange, id1, id2) => {
                validate_typedid_operand_precision_applicability(id1);
                validate_typedid_operand_precision_applicability(id2);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::High {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision must be High, but the \
                         actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // Binary OpCode, result precision must be NotApplicable
            OpCode::Binary(BinaryOpCode::Equal, id1, id2)
            | OpCode::Binary(BinaryOpCode::NotEqual, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThan, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThan, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThanEqual, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThanEqual, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThanVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThanEqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThanVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThanEqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::EqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::NotEqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::LogicalXor, id1, id2) => {
                validate_typedid_operand_precision_applicability(id1);
                validate_typedid_operand_precision_applicability(id2);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::NotApplicable {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision must be NotApplicable, \
                         but the actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // BuiltIn OpCode, result precision must be High
            OpCode::BuiltIn(BuiltInOpCode::TextureSize, args)
            | OpCode::BuiltIn(BuiltInOpCode::UaddCarry, args)
            | OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, args)
            | OpCode::BuiltIn(BuiltInOpCode::NumSamples, args)
            | OpCode::BuiltIn(BuiltInOpCode::AtomicCompSwap, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAdd, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMin, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMax, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAnd, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicOr, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicXor, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicExchange, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicCompSwap, args)
            | OpCode::BuiltIn(BuiltInOpCode::SamplePosition, args) => {
                for arg in args {
                    validate_typedid_operand_precision_applicability(arg);
                }
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != Precision::High {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}, result {:?} precision must be High, but the \
                         actual precision is {:?}",
                        opcode, result, result.precision
                    ));
                }
            }
            // Texture OpCode
            OpCode::Texture(texture_opcode, sampler, coord) => {
                validate_typedid_operand_precision_applicability(sampler);
                validate_typedid_operand_precision_applicability(coord);
                match texture_opcode {
                    TextureOpCode::Implicit { offset, .. } | TextureOpCode::Gather { offset } => {
                        if let Some(valid_offset) = offset {
                            validate_typedid_operand_precision_applicability(valid_offset);
                        }
                    }
                    TextureOpCode::Compare { compare } => {
                        validate_typedid_operand_precision_applicability(compare);
                    }
                    TextureOpCode::Lod { lod, offset, .. } => {
                        validate_typedid_operand_precision_applicability(lod);

                        if let Some(valid_offset) = offset {
                            validate_typedid_operand_precision_applicability(valid_offset);
                        }
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        validate_typedid_operand_precision_applicability(compare);
                        validate_typedid_operand_precision_applicability(lod);
                    }
                    TextureOpCode::Bias { bias, offset, .. } => {
                        validate_typedid_operand_precision_applicability(bias);
                        if let Some(valid_offset) = offset {
                            validate_typedid_operand_precision_applicability(valid_offset);
                        }
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        validate_typedid_operand_precision_applicability(compare);
                        validate_typedid_operand_precision_applicability(bias);
                    }
                    TextureOpCode::Grad { dx, dy, offset, .. } => {
                        validate_typedid_operand_precision_applicability(dx);
                        validate_typedid_operand_precision_applicability(dy);
                        if let Some(valid_offset) = offset {
                            validate_typedid_operand_precision_applicability(valid_offset);
                        }
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        validate_typedid_operand_precision_applicability(component);
                        if let Some(valid_offset) = offset {
                            validate_typedid_operand_precision_applicability(valid_offset);
                        }
                    }
                    TextureOpCode::GatherRef { refz, offset } => {
                        validate_typedid_operand_precision_applicability(refz);
                        if let Some(valid_offset) = offset {
                            validate_typedid_operand_precision_applicability(valid_offset);
                        }
                    }
                }
            }
        }
    }

    // The validate_glsl_result_precision_and_propagation_rules is only called once after GLSL parse
    // and propagate_precision pass. Transformation passes after propagate_precision can change
    // precision that violates the rules in this function, but the transformed IR may still be
    // considered valid.
    // Do not call validate_glsl_result_precision_and_propagation_rules() after other
    // transformation passes.
    fn validate_glsl_result_precision_and_propagation_rules(&self) {
        let mut precision_state = PrecisionState {
            ir_meta: &self.ir.meta,
            function_arg_precisions: HashMap::new(),
            struct_member_precisions: HashMap::new(),
            current_function_id: FunctionId { id: 0 },
        };

        // Prepare a map of functions to the precision of their arguments.
        traverser::visitor::for_each_function(
            &mut precision_state,
            &self.ir.function_entries,
            |precision_state, function_id| {
                let argument_precision = precision_state
                    .ir_meta
                    .get_function(function_id)
                    .params
                    .iter()
                    .map(|param| precision_state.ir_meta.get_variable(param.variable_id).precision)
                    .collect();
                precision_state.function_arg_precisions.insert(function_id, argument_precision);
            },
            |_, _, _, _| traverser::visitor::STOP,
            |_, _| {},
        );

        // Prepare a map of structs to the precision of their members.
        precision_state.ir_meta.all_types().iter().enumerate().for_each(|(index, type_info)| {
            if let Type::Struct(_, fields, _) = type_info {
                let member_precision = fields.iter().map(|field| field.precision).collect();
                precision_state
                    .struct_member_precisions
                    .insert(TypeId { id: index as u32 }, member_precision);
            }
        });

        traverser::visitor::for_each_function(
            &mut precision_state,
            &self.ir.function_entries,
            // PreVisit: cache the current visiting function id
            |precision_state, function_id| {
                precision_state.current_function_id = function_id;
            },
            // BlockVisit: check the precisions in instructions in entry block
            // Return VISIT_SUB_BLOCKS so validate_block_instructions_glsl_precision_rules() is
            // also called on sub-blocks
            |precision_state, entry, _block_kind, _usize| {
                self.validate_block_instructions_glsl_precision_rules(precision_state, entry);
                traverser::visitor::VISIT_SUB_BLOCKS
            },
            // PostVisit: do nothing
            |_, _| {},
        );
    }

    // Called once after GLSL parse and propagate_precision pass.
    // Do not call validate_block_instructions_glsl_precision_rules() after other
    // transformation passes.
    fn validate_block_instructions_glsl_precision_rules(
        &self,
        precision_state: &mut PrecisionState,
        block: &Block,
    ) {
        // Step 1: validate merge output precisions are assigned, after propagate_precision step
        if let Some(expected_merge_input_precision) =
            block.merge_block.as_ref().and_then(|mb| mb.input).map(|input| input.precision)
        {
            let block1 = block.block1.as_ref().expect("block1 can't be none");
            let block1_last_op = block1.get_merge_chain_terminating_op();
            let block2 = block.block2.as_ref().expect("block2 can't be none");
            let block2_last_op = block2.get_merge_chain_terminating_op();

            match block1_last_op {
                OpCode::Merge(Some(merge_output)) => {
                    if expected_merge_input_precision.is_assigned()
                        && !merge_output.precision.is_assigned()
                        && util::is_precision_applicable_to_type(
                            &self.ir.meta,
                            merge_output.type_id,
                        )
                    {
                        self.on_error(format_args!(
                            "invalid instruction {:?}, Merge operand {:?} precision should not be \
                             NotApplicable after propagate_precision step",
                            block1_last_op, merge_output
                        ));
                    }
                }
                _ => {
                    self.on_error(format_args!(
                        "merge block with input expects a block1 ending with Merge(id) instruction"
                    ));
                }
            }

            match block2_last_op {
                OpCode::Merge(Some(merge_output)) => {
                    if expected_merge_input_precision.is_assigned()
                        && !merge_output.precision.is_assigned()
                        && util::is_precision_applicable_to_type(
                            &self.ir.meta,
                            merge_output.type_id,
                        )
                    {
                        self.on_error(format_args!(
                            "invalid instruction {:?}, Merge operand {:?} precision should not be \
                             NotApplicable after propagate_precision step",
                            block2_last_op, merge_output
                        ));
                    }
                }
                _ => {
                    self.on_error(format_args!(
                        "merge block with input expects a block2 ending with Merge(id) instruction"
                    ));
                }
            }
        }

        // Step 2: validate glsl precision rules are applied and propagated properly in all
        // instructions in current block
        for instruction in &block.instructions {
            let (opcode, result) = instruction.get_op_and_result(&self.ir.meta);
            self.validate_instruction_glsl_precision_rules(precision_state, opcode, result);
        }
    }

    // Called once after GLSL parse and propagate_precision pass.
    // Do not call validate_instruction_glsl_precision_rules() after other
    // transformation passes.
    fn validate_instruction_glsl_precision_rules(
        &self,
        precision_state: &PrecisionState,
        opcode: &OpCode,
        result: Option<TypedRegisterId>,
    ) {
        let validate_precision_is_propagated =
            |op_code: &OpCode, precision_to_propagate: Precision, propagate_target: &TypedId| {
                if precision_to_propagate.is_assigned()
                    && !propagate_target.precision.is_assigned()
                    && util::is_precision_applicable_to_type(
                        &self.ir.meta,
                        propagate_target.type_id,
                    )
                {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}, {:?} precision must not be NotApplicable or \
                         Unassigned after the propagate_precision pass",
                        op_code, propagate_target
                    ));
                }
            };

        match opcode {
            // OpCode::Return:
            // - The function return value should have precision propagated
            OpCode::Return(Some(id)) => {
                let expected_function_return_precision =
                    self.ir.meta.get_function(precision_state.current_function_id).return_precision;
                validate_precision_is_propagated(opcode, expected_function_return_precision, id);
            }
            // OpCode::Call:
            // - Function call operands should have precisions propagated
            OpCode::Call(function_id, args) => {
                let expected_function_args_precisions =
                    &precision_state.function_arg_precisions[function_id];

                if args.len() != expected_function_args_precisions.len() {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}. Function call has {} arguments, but the \
                         function declaration has {} arguments",
                        opcode,
                        args.len(),
                        expected_function_args_precisions.len()
                    ));
                }

                for (actual_arg, expected_precision) in
                    args.iter().zip(expected_function_args_precisions)
                {
                    validate_precision_is_propagated(opcode, *expected_precision, actual_arg);
                }
            }
            // OpCode::ConstructStruct:
            // - Operands should have precisions propagated
            OpCode::ConstructStruct(args) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                debug_assert!(self.ir.meta.get_type(result.type_id).is_struct());
                debug_assert!(
                    precision_state.struct_member_precisions.contains_key(&result.type_id)
                );
                let expected_struct_member_precisions =
                    &precision_state.struct_member_precisions[&result.type_id];
                for (actual_member, expected_precision) in
                    args.iter().zip(expected_struct_member_precisions)
                {
                    validate_precision_is_propagated(opcode, *expected_precision, actual_member);
                }
            }
            // OpCode::Store:
            // - The precision of the first operand should propagate to the second operand
            OpCode::Store(ptr_id, val_id) => {
                validate_precision_is_propagated(opcode, ptr_id.precision, val_id);
            }
            // Load OpCode and Alias OpCode:
            // - Result precision should be the same as the operand precision
            OpCode::Load(id) | OpCode::Alias(id) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id.precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the operand {:?}",
                        opcode, result, id
                    ));
                }
            }
            // OpCode::Extract* and OpCode::Access*:
            // - Precision::High should propagate to second operand
            OpCode::ExtractVectorComponentDynamic(_, id2)
            | OpCode::ExtractMatrixColumn(_, id2)
            | OpCode::ExtractArrayElement(_, id2)
            | OpCode::AccessVectorComponentDynamic(_, id2)
            | OpCode::AccessMatrixColumn(_, id2)
            | OpCode::AccessArrayElement(_, id2) => {
                validate_precision_is_propagated(opcode, Precision::High, id2);
            }
            // OpCode::Construct*:
            // - Result precision should match with the operand, unless the operand and result
            // are different in terms of precision applicability.
            // - Result precision should propagate to operand
            OpCode::ConstructScalarFromScalar(id)
            | OpCode::ConstructVectorFromScalar(id)
            | OpCode::ConstructMatrixFromScalar(id)
            | OpCode::ConstructMatrixFromMatrix(id) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id.precision {
                    let precision_applicable_to_result =
                        util::is_precision_applicable_to_type(&self.ir.meta, result.type_id);
                    let precision_applicable_to_operand =
                        util::is_precision_applicable_to_type(&self.ir.meta, id.type_id);
                    if precision_applicable_to_result != precision_applicable_to_operand {
                        // Valid case. For example:
                        // case1: constructing a float from a bool, result has precision, operand
                        // has no precision.
                        // case2: constructing a bool from float, result has no precision, operand
                        // has precision.
                    } else {
                        self.on_error(format_args!(
                            "Invalid instruction {:?}, result {:?} has a different precision than \
                             operand {:?}",
                            opcode, result, id,
                        ));
                    }
                }
                validate_precision_is_propagated(opcode, result.precision, id);
            }
            // OpCode::Construct*:
            // - Result precision should be the highest of all args, unless the operands and result
            //   are different in terms of precision applicability.
            // - Result precision should propagate to all args
            OpCode::ConstructVectorFromMultiple(args)
            | OpCode::ConstructMatrixFromMultiple(args)
            | OpCode::ConstructArray(args) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                let highest_arg_precision = instruction::precision::highest_precision(
                    &mut args.iter().map(|arg| arg.precision),
                );
                if result.precision != highest_arg_precision {
                    let precision_applicable_to_result =
                        util::is_precision_applicable_to_type(&self.ir.meta, result.type_id);
                    let precision_applicable_to_operands =
                        highest_arg_precision != Precision::NotApplicable;
                    if precision_applicable_to_result != precision_applicable_to_operands {
                        // valid case. For example:
                        // constructing a float vec4 from 4 bools.
                    } else {
                        self.on_error(format_args!(
                            "Invalid instruction {:?}, result {:?} has a different precision than \
                             the highest precision {:?} of all operands",
                            opcode, result, highest_arg_precision
                        ));
                    }
                }
                for arg in args {
                    validate_precision_is_propagated(opcode, result.precision, arg);
                }
            }
            // Unary OpCode:
            // - Result precision must be same as the operand
            OpCode::Unary(UnaryOpCode::Negate, id)
            | OpCode::Unary(UnaryOpCode::BitwiseNot, id)
            | OpCode::Unary(UnaryOpCode::PrefixIncrement, id)
            | OpCode::Unary(UnaryOpCode::PrefixDecrement, id)
            | OpCode::Unary(UnaryOpCode::PostfixIncrement, id)
            | OpCode::Unary(UnaryOpCode::PostfixDecrement, id)
            | OpCode::Unary(UnaryOpCode::Radians, id)
            | OpCode::Unary(UnaryOpCode::Degrees, id)
            | OpCode::Unary(UnaryOpCode::Sin, id)
            | OpCode::Unary(UnaryOpCode::Cos, id)
            | OpCode::Unary(UnaryOpCode::Tan, id)
            | OpCode::Unary(UnaryOpCode::Asin, id)
            | OpCode::Unary(UnaryOpCode::Acos, id)
            | OpCode::Unary(UnaryOpCode::Atan, id)
            | OpCode::Unary(UnaryOpCode::Sinh, id)
            | OpCode::Unary(UnaryOpCode::Cosh, id)
            | OpCode::Unary(UnaryOpCode::Tanh, id)
            | OpCode::Unary(UnaryOpCode::Asinh, id)
            | OpCode::Unary(UnaryOpCode::Acosh, id)
            | OpCode::Unary(UnaryOpCode::Atanh, id)
            | OpCode::Unary(UnaryOpCode::Exp, id)
            | OpCode::Unary(UnaryOpCode::Log, id)
            | OpCode::Unary(UnaryOpCode::Exp2, id)
            | OpCode::Unary(UnaryOpCode::Log2, id)
            | OpCode::Unary(UnaryOpCode::Sqrt, id)
            | OpCode::Unary(UnaryOpCode::Inversesqrt, id)
            | OpCode::Unary(UnaryOpCode::Abs, id)
            | OpCode::Unary(UnaryOpCode::Sign, id)
            | OpCode::Unary(UnaryOpCode::Floor, id)
            | OpCode::Unary(UnaryOpCode::Trunc, id)
            | OpCode::Unary(UnaryOpCode::Round, id)
            | OpCode::Unary(UnaryOpCode::RoundEven, id)
            | OpCode::Unary(UnaryOpCode::Ceil, id)
            | OpCode::Unary(UnaryOpCode::Fract, id)
            | OpCode::Unary(UnaryOpCode::Length, id)
            | OpCode::Unary(UnaryOpCode::Normalize, id)
            | OpCode::Unary(UnaryOpCode::Transpose, id)
            | OpCode::Unary(UnaryOpCode::Determinant, id)
            | OpCode::Unary(UnaryOpCode::Inverse, id)
            | OpCode::Unary(UnaryOpCode::DFdx, id)
            | OpCode::Unary(UnaryOpCode::DFdy, id)
            | OpCode::Unary(UnaryOpCode::Fwidth, id)
            | OpCode::Unary(UnaryOpCode::InterpolateAtCentroid, id)
            | OpCode::Unary(UnaryOpCode::PixelLocalLoadANGLE, id) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id.precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} precision is different from \
                         operand {:?}",
                        opcode, result, id
                    ));
                }
            }
            // Unary OpCode: Precision::High should propagate to operand
            OpCode::Unary(UnaryOpCode::PackSnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::PackHalf2x16, id)
            | OpCode::Unary(UnaryOpCode::PackUnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::PackUnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::PackSnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::FloatBitsToInt, id)
            | OpCode::Unary(UnaryOpCode::FloatBitsToUint, id)
            | OpCode::Unary(UnaryOpCode::IntBitsToFloat, id)
            | OpCode::Unary(UnaryOpCode::UintBitsToFloat, id)
            | OpCode::Unary(UnaryOpCode::UnpackSnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::UnpackUnorm2x16, id)
            | OpCode::Unary(UnaryOpCode::BitfieldReverse, id)
            | OpCode::Unary(UnaryOpCode::AtomicCounter, id)
            | OpCode::Unary(UnaryOpCode::AtomicCounterIncrement, id)
            | OpCode::Unary(UnaryOpCode::AtomicCounterDecrement, id)
            | OpCode::Unary(UnaryOpCode::ImageSize, id)
            | OpCode::Unary(UnaryOpCode::UnpackHalf2x16, id)
            | OpCode::Unary(UnaryOpCode::UnpackUnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::UnpackSnorm4x8, id)
            | OpCode::Unary(UnaryOpCode::BitCount, id)
            | OpCode::Unary(UnaryOpCode::FindLSB, id)
            | OpCode::Unary(UnaryOpCode::FindMSB, id)
            | OpCode::Unary(UnaryOpCode::Isnan, id)
            | OpCode::Unary(UnaryOpCode::Isinf, id) => {
                validate_precision_is_propagated(opcode, Precision::High, id);
            }
            // Binary OpCode
            // - Result precision must be the higher precision of the operands
            OpCode::Binary(BinaryOpCode::BitwiseOr, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitwiseXor, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitwiseAnd, id1, id2) => {
                let higher_arg_precision =
                    instruction::precision::higher_precision(id1.precision, id2.precision);
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != higher_arg_precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the higher precision {:?} of the two operands",
                        opcode, result, higher_arg_precision
                    ));
                }
            }
            // Binary OpCode
            // - Result precision must be the higher precision of the operands
            // - Result precision should propagate to both operands
            OpCode::Binary(BinaryOpCode::Add, id1, id2)
            | OpCode::Binary(BinaryOpCode::Sub, id1, id2)
            | OpCode::Binary(BinaryOpCode::Mul, id1, id2)
            | OpCode::Binary(BinaryOpCode::VectorTimesScalar, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixTimesScalar, id1, id2)
            | OpCode::Binary(BinaryOpCode::VectorTimesMatrix, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixTimesVector, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixTimesMatrix, id1, id2)
            | OpCode::Binary(BinaryOpCode::Div, id1, id2)
            | OpCode::Binary(BinaryOpCode::IMod, id1, id2)
            | OpCode::Binary(BinaryOpCode::Atan, id1, id2)
            | OpCode::Binary(BinaryOpCode::Pow, id1, id2)
            | OpCode::Binary(BinaryOpCode::Mod, id1, id2)
            | OpCode::Binary(BinaryOpCode::Min, id1, id2)
            | OpCode::Binary(BinaryOpCode::Max, id1, id2)
            | OpCode::Binary(BinaryOpCode::Step, id1, id2)
            | OpCode::Binary(BinaryOpCode::Modf, id1, id2)
            | OpCode::Binary(BinaryOpCode::Distance, id1, id2)
            | OpCode::Binary(BinaryOpCode::Dot, id1, id2)
            | OpCode::Binary(BinaryOpCode::Cross, id1, id2)
            | OpCode::Binary(BinaryOpCode::Reflect, id1, id2)
            | OpCode::Binary(BinaryOpCode::MatrixCompMult, id1, id2)
            | OpCode::Binary(BinaryOpCode::OuterProduct, id1, id2) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                let higher_arg_precision =
                    instruction::precision::higher_precision(id1.precision, id2.precision);
                if result.precision != higher_arg_precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the higher precision {:?} of the two operands",
                        opcode, result, higher_arg_precision
                    ));
                }
                validate_precision_is_propagated(opcode, result.precision, id1);
                validate_precision_is_propagated(opcode, result.precision, id2);
            }
            // Binary OpCode
            // - Result precision should propagate to second operand
            OpCode::Binary(BinaryOpCode::AtomicAdd, _, id2)
            | OpCode::Binary(BinaryOpCode::AtomicMin, _, id2)
            | OpCode::Binary(BinaryOpCode::AtomicMax, _, id2)
            | OpCode::Binary(BinaryOpCode::AtomicAnd, _, id2)
            | OpCode::Binary(BinaryOpCode::AtomicOr, _, id2)
            | OpCode::Binary(BinaryOpCode::AtomicXor, _, id2)
            | OpCode::Binary(BinaryOpCode::AtomicExchange, _, id2) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                validate_precision_is_propagated(opcode, result.precision, id2);
            }
            // Binary OpCode:
            // - Precision::High should propagate to both operands
            OpCode::Binary(BinaryOpCode::Frexp, id1, id2)
            | OpCode::Binary(BinaryOpCode::Ldexp, id1, id2) => {
                validate_precision_is_propagated(opcode, Precision::High, id1);
                validate_precision_is_propagated(opcode, Precision::High, id2);
            }
            // Binary OpCode:
            // - Precision of one operand should propagate to the other operand
            OpCode::Binary(BinaryOpCode::Equal, id1, id2)
            | OpCode::Binary(BinaryOpCode::NotEqual, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThan, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThan, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThanEqual, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThanEqual, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThanVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::LessThanEqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThanVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::GreaterThanEqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::EqualVec, id1, id2)
            | OpCode::Binary(BinaryOpCode::NotEqualVec, id1, id2) => {
                validate_precision_is_propagated(opcode, id1.precision, id2);
                validate_precision_is_propagated(opcode, id2.precision, id1);
            }
            // Binary OpCode
            // - Result precision must be same as the first operand precision
            // - Result precision should propagate to both operands
            OpCode::Binary(BinaryOpCode::BitShiftLeft, id1, id2)
            | OpCode::Binary(BinaryOpCode::BitShiftRight, id1, id2)
            | OpCode::Binary(BinaryOpCode::InterpolateAtOffset, id1, id2) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id1.precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the first operand {:?}",
                        opcode, result, id1
                    ));
                }
                validate_precision_is_propagated(opcode, result.precision, id1);
                validate_precision_is_propagated(opcode, result.precision, id2);
            }
            // Binary OpCode InterpolateAtSample:
            // - Result precision must be same as the first operand precision
            // - Result precision should propagate to first operand
            // - Precision::High should propagate to second operand
            OpCode::Binary(BinaryOpCode::InterpolateAtSample, id1, id2) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != id1.precision {
                    self.on_error(format_args!(
                        "Invalid instruction: {:?}, result {:?} has a different precision than \
                         the first operand {:?}",
                        opcode, result, id1
                    ));
                }
                validate_precision_is_propagated(opcode, result.precision, id1);
                validate_precision_is_propagated(opcode, Precision::High, id2);
            }
            // BuiltIn OpCode
            // - Result precision must be the same as the first operand precision
            OpCode::BuiltIn(BuiltInOpCode::SubpassLoad, args) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != args.first().unwrap().precision {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}, result {:?} precision must be the same as the \
                         first operand {:?}",
                        opcode,
                        result,
                        args.first().unwrap(),
                    ));
                }
            }
            // BuiltIn OpCode
            // - Result precision must be the same as the first operand precision
            // - Result precision should propagate to all operands
            OpCode::BuiltIn(BuiltInOpCode::BitfieldExtract, args)
            | OpCode::BuiltIn(BuiltInOpCode::BitfieldInsert, args)
            | OpCode::BuiltIn(BuiltInOpCode::InterpolateAtCenter, args)
            | OpCode::BuiltIn(BuiltInOpCode::TextureQueryLod, args)
            | OpCode::BuiltIn(BuiltInOpCode::TexelFetch, args)
            | OpCode::BuiltIn(BuiltInOpCode::TexelFetchOffset, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageLoad, args) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                if result.precision != args.first().unwrap().precision {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}, result {:?} precision must be the same as the \
                         first operand {:?}",
                        opcode,
                        result,
                        args.first().unwrap(),
                    ));
                }

                for arg in args {
                    validate_precision_is_propagated(opcode, result.precision, arg);
                }
            }
            // BuiltIn OpCode
            // - Result precision must be the highest precision of all operands
            // - Result precision should propagate to all operands
            OpCode::BuiltIn(BuiltInOpCode::Clamp, args)
            | OpCode::BuiltIn(BuiltInOpCode::Mix, args)
            | OpCode::BuiltIn(BuiltInOpCode::Smoothstep, args)
            | OpCode::BuiltIn(BuiltInOpCode::Fma, args)
            | OpCode::BuiltIn(BuiltInOpCode::Faceforward, args)
            | OpCode::BuiltIn(BuiltInOpCode::Refract, args)
            | OpCode::BuiltIn(BuiltInOpCode::Rgb2Yuv, args)
            | OpCode::BuiltIn(BuiltInOpCode::Yuv2Rgb, args)
            | OpCode::BuiltIn(BuiltInOpCode::Saturate, args) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                let highest_arg_precision = instruction::precision::highest_precision(
                    &mut args.iter().map(|arg| arg.precision),
                );
                if result.precision != highest_arg_precision {
                    self.on_error(format_args!(
                        "Invalid instruction {:?}, result {:?} precision must be the highest \
                         precision {:?} of all operands",
                        opcode, result, highest_arg_precision
                    ));
                }
                for arg in args {
                    validate_precision_is_propagated(opcode, result.precision, arg);
                }
            }
            // OpCode::BuiltIn: Precision::High should propagate to last operand
            OpCode::BuiltIn(BuiltInOpCode::TextureSize, args) => {
                validate_precision_is_propagated(opcode, Precision::High, args.last().unwrap());
            }
            // OpCode::BuiltIn:
            // Precision::High should propagate to first operand
            // Precision::High should propagate to second operand
            // Precision::Low should propagate to third operand
            OpCode::BuiltIn(BuiltInOpCode::UaddCarry, args)
            | OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, args) => {
                validate_precision_is_propagated(opcode, Precision::High, &args[0]);
                validate_precision_is_propagated(opcode, Precision::High, &args[1]);
                validate_precision_is_propagated(opcode, Precision::Low, &args[2]);
            }
            // OpCode::BuiltIn
            // - Result precision should propagate to all operands
            OpCode::BuiltIn(BuiltInOpCode::AtomicCompSwap, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAdd, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMin, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMax, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAnd, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicOr, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicXor, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicExchange, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicCompSwap, args)
            | OpCode::BuiltIn(BuiltInOpCode::SamplePosition, args) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                for arg in args {
                    validate_precision_is_propagated(opcode, result.precision, arg);
                }
            }
            // OpCode::BuiltIn:
            // - operand[0] precision should propagate to operand[1]
            OpCode::BuiltIn(BuiltInOpCode::PixelLocalStoreANGLE, args) => {
                validate_precision_is_propagated(opcode, args.first().unwrap().precision, &args[1]);
            }

            // OpCode::BuiltIn:
            // - operand[0] precision should propagate to all other operands
            OpCode::BuiltIn(BuiltInOpCode::ImageStore, args) => {
                for arg in args {
                    validate_precision_is_propagated(opcode, args.first().unwrap().precision, arg);
                }
            }

            // OpCode::BuiltIn:
            // - Precision::High should propagate to first two operands
            OpCode::BuiltIn(BuiltInOpCode::UmulExtended, args)
            | OpCode::BuiltIn(BuiltInOpCode::ImulExtended, args) => {
                validate_precision_is_propagated(opcode, Precision::High, &args[0]);
                validate_precision_is_propagated(opcode, Precision::High, &args[1]);
            }

            // OpCode::Texture:
            // - Result precision should propagate to coord
            OpCode::Texture(texture_opcode, _, coord) => {
                let result = result.expect("Expect a TypedRegisterId result for the OpCode");
                validate_precision_is_propagated(opcode, result.precision, coord);

                match texture_opcode {
                    // Result precision should propagate to offset
                    TextureOpCode::Implicit { offset, .. }
                    | TextureOpCode::Gather { offset }
                    | TextureOpCode::GatherComponent { offset, .. } => {
                        if let Some(valid_offset) = offset {
                            validate_precision_is_propagated(
                                opcode,
                                result.precision,
                                valid_offset,
                            );
                        }
                    }
                    // Result precision should propagate to compare
                    TextureOpCode::Compare { compare } => {
                        validate_precision_is_propagated(opcode, result.precision, compare);
                    }

                    // Result precision should propagate to lod
                    TextureOpCode::Lod { lod, .. } => {
                        validate_precision_is_propagated(opcode, result.precision, lod);
                    }
                    // Result precision should propagate to compare and lod
                    TextureOpCode::CompareLod { compare, lod } => {
                        validate_precision_is_propagated(opcode, result.precision, compare);
                        validate_precision_is_propagated(opcode, result.precision, lod);
                    }

                    // Result precision should propagate to bias
                    TextureOpCode::Bias { bias, .. } => {
                        validate_precision_is_propagated(opcode, result.precision, bias);
                    }

                    // Result precision should propagate to compare and bias
                    TextureOpCode::CompareBias { compare, bias } => {
                        validate_precision_is_propagated(opcode, result.precision, compare);
                        validate_precision_is_propagated(opcode, result.precision, bias);
                    }
                    // Result precision should propagate to dx and dy
                    TextureOpCode::Grad { dx, dy, .. } => {
                        validate_precision_is_propagated(opcode, result.precision, dx);
                        validate_precision_is_propagated(opcode, result.precision, dy);
                    }
                    // Result precision should propagate to refz
                    TextureOpCode::GatherRef { refz, .. } => {
                        validate_precision_is_propagated(opcode, result.precision, refz);
                    }
                }
            }
            _ => {}
        }
    }
}
