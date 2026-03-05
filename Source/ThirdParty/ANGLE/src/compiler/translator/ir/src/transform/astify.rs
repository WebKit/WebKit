// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Prepare the IR for turning into AST (be it ANGLE's legacy AST, or actual GLSL/etc text):
//
// - Remove merge block inputs and replace them with temp variables.
// - Use temporary variables for ops that have side effect but whose results are used multiple
//   times.
// - Use temporary variables for swizzles applied to swizzles.  This is normally folded, except if
//   the first swizzle is applied to a pointer, it's loaded, and then another swizzle is applied to
//   the loaded value.
// - Use temporary variables for constants whose precision is higher than the other operands in the
//   instruction so that the constant's precision is not lost.
// - TODO(http://anglebug.com/349994211): Use temporary variables for ops whose result precision
//   does not match what would have automatically been derived for it... it's a rather niche
//   optimization, as precision mismatch can happen only by IR transformations.  Current AST doesn't
//   do this (so GLSL output would have incorrect precisions for example), although intermediate
//   nodes' precisions are retained during transformation so SPIR-V output is correct. Without this
//   change, SPIR-V output from AST after IR is incorrect until SPIR-V is directly generated from
//   IR.
// - Replicate the continue block before every `continue`, if any.
// - Replicate the do-loop condition block before every `continue`, if any, but with it ending with
//   `if (!condition) break;`.  To support `continue` inside `switch`, a bool variable is created
//   for those that include a `continue` statement, which is set to true before the `break` that's
//   generated during this replication.  After the switch, an `if (propagate_break) break;` is
//   added.
use crate::ir::*;
use crate::*;

#[cfg_attr(debug_assertions, derive(Debug))]
struct RegisterInfo {
    // How many times the register is read from.
    read_count: u32,
    // Whether the calculation that led to this ID has side effect.  If the ID is read from
    // multiple times, it cannot be replicated in AST if it has side effects.
    has_side_effect: bool,
    // Whether the calculation that led to this ID is complex.  If the ID is read from
    // multiple times, it is inefficient in AST to replicate it.
    is_complex: bool,
    // Whether the register must be marked as having a side effect if ever accessed.  See
    // `preprocess_block_registers`'s comment on OpCode::Load.
    mark_side_effect_if_read: bool,
}

impl RegisterInfo {
    fn new() -> RegisterInfo {
        RegisterInfo {
            read_count: 0,
            has_side_effect: false,
            is_complex: false,
            mark_side_effect_if_read: false,
        }
    }
}

#[cfg_attr(debug_assertions, derive(Debug))]
struct BreakInfo {
    // Whether this is a switch block
    is_switch: bool,
    // What is the variable to propagate `break`s.
    propagate_break_var: Option<VariableId>,
}

impl BreakInfo {
    fn new_loop() -> BreakInfo {
        BreakInfo { is_switch: false, propagate_break_var: None }
    }

    fn new_switch() -> BreakInfo {
        BreakInfo { is_switch: true, propagate_break_var: None }
    }
}

#[cfg_attr(debug_assertions, derive(Debug))]
struct State<'a> {
    ir_meta: &'a mut IRMeta,
    // Used to know when temporary variables are needed
    register_info: HashMap<RegisterId, RegisterInfo>,
    // List of registers without side effect that are not necessarily cached in a temp variable,
    // see comment in `preprocess_block_registers` for details.
    uncached_registers: Vec<RegisterId>,
    // Used to replicate the continue block of for loops before each `continue`
    // instruction.  `continue_stack` includes the continue block of constructs that
    // interact with a `continue` instruction (Loop and DoLoop).
    continue_stack: Vec<Option<Block>>,
    // Used to replicate the condition block of do-loops before each `continue`
    // instruction.
    //
    // `break_stack` tracks constructs that interact with a `break` instruction (Loop,
    // DoLoop and Switch).  It contains whether the construct is a Switch block, and if so
    // whether it needs a variable for an indirect break and what that variable is.
    //
    // `condition_stack` includes the condition block of DoLoop, or None for Loops.  The
    // condition block is changed to include `if (!condition) break` followed by
    // `NextBlock`, instead of `LoopIf`.
    break_stack: Vec<BreakInfo>,
    condition_stack: Vec<Option<Block>>,
}

pub fn run(ir: &mut IR) {
    let mut state = State {
        ir_meta: &mut ir.meta,
        register_info: HashMap::new(),
        uncached_registers: Vec::new(),
        continue_stack: Vec::new(),
        break_stack: Vec::new(),
        condition_stack: Vec::new(),
    };

    // Pre-process the registers to determine when temporary variables are needed.
    preprocess_registers(&mut state, &ir.function_entries);

    // First, create temporary variables for the result of instructions that have side
    // effects and yet are read from multiple times.
    traverser::transformer::for_each_instruction(
        &mut state,
        &mut ir.function_entries,
        &|state, instruction| transform_instruction(state, instruction),
    );

    // Then create variables for merge block inputs as that is not representable in the
    // AST.  No need to try and be smart with them, such as producing ?:, && or ||.
    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, _, entry| {
            traverser::transformer::for_each_block(
                state,
                entry,
                &|state, block| replace_merge_input_with_variable(state, block),
                &|_, block| block,
            );
        },
    );

    // Finally, duplicate the continue block, if any, before each `continue` branch, because
    // it may be too complicated to be placed inside a `for ()` expression.  Afterwards,
    // the IR no longer has continue blocks.
    //
    // At the same time, duplicate the condition block of do-loops before each `continue`
    // branch.
    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, _, entry| {
            traverser::transformer::for_each_block(
                state,
                entry,
                &|state, block| transform_continue_instructions_pre_visit(state, block),
                &|state, block| transform_continue_instructions_post_visit(state, block),
            );
        },
    );
}

fn get_op_read_ids(opcode: &OpCode) -> Vec<TypedId> {
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
        | OpCode::Merge(None) => vec![],
        OpCode::Call(_, params)
        | OpCode::ConstructVectorFromMultiple(params)
        | OpCode::ConstructMatrixFromMultiple(params)
        | OpCode::ConstructStruct(params)
        | OpCode::ConstructArray(params)
        | OpCode::BuiltIn(_, params) => params.clone(),
        &OpCode::Return(Some(id))
        | &OpCode::Merge(Some(id))
        | &OpCode::If(id)
        | &OpCode::LoopIf(id)
        | &OpCode::Switch(id, _)
        | &OpCode::ExtractVectorComponent(id, _)
        | &OpCode::ExtractVectorComponentMulti(id, _)
        | &OpCode::ExtractStructField(id, _)
        | &OpCode::ConstructScalarFromScalar(id)
        | &OpCode::ConstructVectorFromScalar(id)
        | &OpCode::ConstructMatrixFromScalar(id)
        | &OpCode::ConstructMatrixFromMatrix(id)
        | &OpCode::AccessVectorComponent(id, _)
        | &OpCode::AccessVectorComponentMulti(id, _)
        | &OpCode::AccessStructField(id, _)
        | &OpCode::Load(id)
        | &OpCode::Unary(_, id) => vec![id],
        &OpCode::ExtractVectorComponentDynamic(lhs, rhs)
        | &OpCode::ExtractMatrixColumn(lhs, rhs)
        | &OpCode::ExtractArrayElement(lhs, rhs)
        | &OpCode::AccessVectorComponentDynamic(lhs, rhs)
        | &OpCode::AccessMatrixColumn(lhs, rhs)
        | &OpCode::AccessArrayElement(lhs, rhs)
        | &OpCode::Store(lhs, rhs)
        | &OpCode::Binary(_, lhs, rhs) => vec![lhs, rhs],
        &OpCode::Texture(ref texture_op, sampler, coord) => {
            let mut read_ids = Vec::with_capacity(4);
            read_ids.push(sampler);
            read_ids.push(coord);
            match texture_op {
                &TextureOpCode::Implicit { is_proj: _, offset }
                | &TextureOpCode::Gather { offset } => {
                    offset.inspect(|&id| read_ids.push(id));
                }
                &TextureOpCode::Compare { compare } => {
                    read_ids.push(compare);
                }
                &TextureOpCode::Lod { is_proj: _, lod, offset } => {
                    read_ids.push(lod);
                    offset.inspect(|&id| read_ids.push(id));
                }
                &TextureOpCode::CompareLod { compare, lod } => {
                    read_ids.push(compare);
                    read_ids.push(lod);
                }
                &TextureOpCode::Bias { is_proj: _, bias, offset } => {
                    read_ids.push(bias);
                    offset.inspect(|&id| read_ids.push(id));
                }
                &TextureOpCode::CompareBias { compare, bias } => {
                    read_ids.push(compare);
                    read_ids.push(bias);
                }
                &TextureOpCode::Grad { is_proj: _, dx, dy, offset } => {
                    read_ids.push(dx);
                    read_ids.push(dy);
                    offset.inspect(|&id| read_ids.push(id));
                }
                &TextureOpCode::GatherComponent { component, offset } => {
                    read_ids.push(component);
                    offset.inspect(|&id| read_ids.push(id));
                }
                &TextureOpCode::GatherRef { refz, offset } => {
                    read_ids.push(refz);
                    offset.inspect(|&id| read_ids.push(id));
                }
            };
            read_ids
        }
        &OpCode::Alias(_) => {
            panic!("Internal error: Aliases must have been resolved before text generation")
        }
    }
}

// Return whether Loads and other expressions without side effect can stay deferred after this op
// or not.  Additionally, it returns whether the op only reads from its arguments or not; if it
// only reads from the arguments, then the expressions don't need to be cached before the op
// itself.
fn can_reorder_expressions_after_op(opcode: &OpCode) -> (bool, bool) {
    match opcode {
        OpCode::Store(..) => (false, true),
        OpCode::Call(..)
        | OpCode::Unary(UnaryOpCode::PrefixIncrement, _)
        | OpCode::Unary(UnaryOpCode::PrefixDecrement, _)
        | OpCode::Unary(UnaryOpCode::PostfixIncrement, _)
        | OpCode::Unary(UnaryOpCode::PostfixDecrement, _)
        | OpCode::Unary(UnaryOpCode::AtomicCounterIncrement, _)
        | OpCode::Unary(UnaryOpCode::AtomicCounterDecrement, _)
        | OpCode::Binary(BinaryOpCode::Modf, _, _)
        | OpCode::Binary(BinaryOpCode::Frexp, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicAdd, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicMin, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicMax, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicAnd, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicOr, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicXor, _, _)
        | OpCode::Binary(BinaryOpCode::AtomicExchange, _, _)
        | OpCode::BuiltIn(BuiltInOpCode::UaddCarry, _)
        | OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, _)
        | OpCode::BuiltIn(BuiltInOpCode::UmulExtended, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImulExtended, _)
        | OpCode::BuiltIn(BuiltInOpCode::AtomicCompSwap, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageStore, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAdd, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMin, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMax, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAnd, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicOr, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicXor, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicExchange, _)
        | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicCompSwap, _)
        | OpCode::BuiltIn(BuiltInOpCode::PixelLocalStoreANGLE, _) => {
            // Functions may mutate global variables.  They may also mutate out and inout
            // variables.  For simplicity, we don't try to track exactly what they mutate, and
            // assume any load that has been deferred might be mutated by the call (and so, cannot
            // be deferred).
            // Similarly, for built-ins that modify memory, we don't track what exactly they might
            // have modified.
            (false, false)
        }
        OpCode::BuiltIn(BuiltInOpCode::BeginInvocationInterlockNV, _)
        | OpCode::BuiltIn(BuiltInOpCode::EndInvocationInterlockNV, _)
        | OpCode::BuiltIn(BuiltInOpCode::BeginFragmentShaderOrderingINTEL, _)
        | OpCode::BuiltIn(BuiltInOpCode::BeginInvocationInterlockARB, _)
        | OpCode::BuiltIn(BuiltInOpCode::EndInvocationInterlockARB, _) => {
            // Reads should not be deferred to after the interlock operation (especially _end_ of
            // the interlock).
            (false, false)
        }
        _ => (true, false),
    }
}

fn clear_uncached_registers(
    uncached_registers: &mut Vec<RegisterId>,
    register_info: &mut HashMap<RegisterId, RegisterInfo>,
) {
    uncached_registers.iter().for_each(|id| {
        register_info.get_mut(id).unwrap().mark_side_effect_if_read = true;
    });
    uncached_registers.clear();
}

fn preprocess_block_registers(state: &mut State, block: &Block) {
    // Add an unassuming entry for the merge input, if any.
    if let Some(input) = block.input {
        state.register_info.entry(input.id).or_insert(RegisterInfo::new());
    }

    for instruction in &block.instructions {
        let (opcode, result) = instruction.get_op_and_result(state.ir_meta);

        // If any loads are deferred, make sure they are not done after instructions that might
        // have mutated their memory.  This is done by marking them to be cached if ever read
        // again; if never read after this point, it was fine to let them be read on use before
        // this point.
        let (can_reorder_expressions, op_only_reads_from_args) =
            can_reorder_expressions_after_op(opcode);
        if !can_reorder_expressions && !op_only_reads_from_args {
            clear_uncached_registers(&mut state.uncached_registers, &mut state.register_info);
        }

        // Mark every potentially-register Id in the arguments of the opcode as being
        // accessed.
        for id in get_op_read_ids(opcode) {
            if let Id::Register(id) = id.id {
                let read_register_info = state.register_info.get_mut(&id).unwrap();
                read_register_info.read_count += 1;

                if read_register_info.mark_side_effect_if_read {
                    read_register_info.has_side_effect = true;
                }
            }
        }

        if !can_reorder_expressions && op_only_reads_from_args {
            clear_uncached_registers(&mut state.uncached_registers, &mut state.register_info);
        }

        // If the instruction has a side-effect, mark its resulting ID as having side
        // effect.  Similarly, if it is complex, mark it as such.
        if let Some(result_id) = result {
            // Add an unassuming entry for the result.
            let result_info =
                state.register_info.entry(result_id.id).or_insert(RegisterInfo::new());

            match opcode {
                OpCode::Call(..)
                | OpCode::Unary(UnaryOpCode::PrefixIncrement, _)
                | OpCode::Unary(UnaryOpCode::PrefixDecrement, _)
                | OpCode::Unary(UnaryOpCode::PostfixIncrement, _)
                | OpCode::Unary(UnaryOpCode::PostfixDecrement, _)
                | OpCode::Unary(UnaryOpCode::AtomicCounter, _)
                | OpCode::Unary(UnaryOpCode::AtomicCounterIncrement, _)
                | OpCode::Unary(UnaryOpCode::AtomicCounterDecrement, _)
                | OpCode::Unary(UnaryOpCode::PixelLocalLoadANGLE, _)
                | OpCode::Binary(BinaryOpCode::Modf, _, _)
                | OpCode::Binary(BinaryOpCode::Frexp, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicAdd, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicMin, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicMax, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicAnd, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicOr, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicXor, _, _)
                | OpCode::Binary(BinaryOpCode::AtomicExchange, _, _)
                | OpCode::BuiltIn(BuiltInOpCode::UaddCarry, _)
                | OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, _)
                | OpCode::BuiltIn(BuiltInOpCode::UmulExtended, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImulExtended, _)
                | OpCode::BuiltIn(BuiltInOpCode::AtomicCompSwap, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageLoad, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAdd, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMin, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicMax, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicAnd, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicOr, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicXor, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicExchange, _)
                | OpCode::BuiltIn(BuiltInOpCode::ImageAtomicCompSwap, _) => {
                    // TODO(http://anglebug.com/349994211): for now, assume every function call has
                    // a side effect.  This can be optimized with a prepass going over functions
                    // and checking if they have side effect.  AST assumes user functions have side
                    // effects, and mostly uses isKnownNotToHaveSideEffects for built-ins, which
                    // are separately checked here.  Some internal transformations mark a function
                    // as no-side effect, but no real benefit comes from that IMO.  This is
                    // probably fine as-is.
                    //
                    // TODO(http://anglebug.com/349994211): move the above logic (of which ops have
                    // side effect) to a query in OpCode or IRMeta.  Later, when DCE is performed
                    // the same information is needed.
                    result_info.has_side_effect = true;
                    result_info.is_complex = true;
                }
                OpCode::AccessVectorComponent(..)
                | OpCode::AccessVectorComponentMulti(..)
                | OpCode::AccessVectorComponentDynamic(..)
                | OpCode::AccessMatrixColumn(..)
                | OpCode::AccessStructField(..)
                | OpCode::AccessArrayElement(..)
                | OpCode::ExtractVectorComponentDynamic(..)
                | OpCode::ExtractMatrixColumn(..)
                | OpCode::ExtractStructField(..)
                | OpCode::ExtractArrayElement(..)
                | OpCode::Load(..) => {
                    // Consider Access* instructions not complex and not having side effect, it's
                    // impossible to cache the result in the AST.
                    // Most Extract* instructions also don't need to be cached in a variable.
                    // For load, we would ideally not want to cache the result in a variable only
                    // to have to load from it on every use again.
                }
                OpCode::ExtractVectorComponent(vector_id, _)
                | OpCode::ExtractVectorComponentMulti(vector_id, _) => {
                    // Shading languages typically don't expect swizzle applied to swizzle, so
                    // check to see if the value being swizzled was itself loaded from a swizzled
                    // pointer, in which case mark the load operation as "complex" with "multiple
                    // reads" so it gets cached in a temporary.
                    let load_id = vector_id.id.get_register();
                    if let OpCode::Load(pointer_id) = state.ir_meta.get_instruction(load_id).op
                        && let Id::Register(pointer_id) = pointer_id.id
                        && matches!(
                            state.ir_meta.get_instruction(pointer_id).op,
                            OpCode::AccessVectorComponentMulti(..)
                        )
                    {
                        let load_register_info = state.register_info.get_mut(&load_id).unwrap();
                        // The ExtractVectorComponent* instruction has already counted as
                        // one read, so one more is enough.
                        load_register_info.read_count += 1;
                        debug_assert!(load_register_info.read_count > 1);
                        load_register_info.is_complex = true;
                    }
                }
                _ => {
                    // For everything else, consider the result complex so the logic is not
                    // duplicated.  This can cover everything from a+b to texture calls
                    // etc.
                    result_info.is_complex = true;
                }
            };

            // When a register does not have a side effect, it refers to a temporary expression
            // that is yet to be used.  For example, take `a + b`.  To produce smaller more
            // readable shader, the result of this temporary is not immediately cached in a
            // temp variable, it may never need to be cached, for example if `a + b` is going to be
            // stored in a variable right away and this register is never referenced afterwards.
            //
            // However, these expressions cannot stay uncached forever.  Take the following example:
            //
            //   r1 = Load global_variable
            //   r2 = Add r1 ...
            //        Call function_that_changes_global_variable
            //        Store local_variable r2
            //
            // In the above scenario, if the load from `r1` is deferred until the expression
            // corresponding to `r2` is used (in assignment to `local_variable`), the AST would
            // look like this:
            //
            //     function_that_changes_global_variable();
            //     local_variable = global_variable + ...;
            //
            // This is incorrect because `r1` contained the value of `global_variable` _before_ the
            // function call, not after.  Instead, the following would be correct:
            //
            //     temp_variable = global_variable + ...;
            //     function_that_changes_global_variable();
            //     local_variable = temp_variable;
            //
            // To produce nicer AST, these side-effect-less results are not considered complex /
            // with side effect initially.  If the results are read from right away, the AST can
            // reference the expression directly.
            //
            // However, if any instruction can modify a variable (i.e. Store, some
            // built-ins, function calls), the result of the expression needs to be cached in a
            // temporary variable if it's ever accessed afterwards.
            //
            // To implement this, the register is marked as being deferred here in a list, and
            // later if a mutating instruction is encountered, all these registers are marked as
            // needing to be cached if read from again.  For simplicity, we don't track exactly
            // which variables may be modified by the mutating instruction.
            if !state.register_info.get(&result_id.id).unwrap().has_side_effect {
                let result_type = state.ir_meta.get_type(result_id.type_id);
                let can_be_cached = !matches!(
                    result_type,
                    Type::Scalar(BasicType::AtomicCounter)
                        | Type::Image(..)
                        | Type::UnsizedArray(_)
                        | Type::Pointer(_)
                );
                if can_be_cached {
                    state.uncached_registers.push(result_id.id);
                }
            }
        }
    }
}

fn preprocess_registers(state: &mut State, function_entries: &[Option<Block>]) {
    traverser::visitor::for_each_function(
        state,
        function_entries,
        |_, _| {},
        |state, block, _, _| {
            preprocess_block_registers(state, block);
            traverser::visitor::VISIT_SUB_BLOCKS
        },
        |_, _| {},
    );
}

fn has_constants_with_higher_precision(operands: &[TypedId]) -> Option<Precision> {
    // Check if the highest precision of the constant operands is higher than the highest precision
    // of the non-constant operands.  In that case, any constant that has a precision higher than
    // the non-constant operands must be placed in a temporary variable.
    let (highest_constant_precision, highest_non_constant_precision) = operands.iter().fold(
        (Precision::NotApplicable, Precision::NotApplicable),
        |(constant_precision, non_constant_precision), operand| {
            if operand.id.is_constant() {
                (
                    instruction::precision::higher_precision(constant_precision, operand.precision),
                    non_constant_precision,
                )
            } else {
                (
                    constant_precision,
                    instruction::precision::higher_precision(
                        non_constant_precision,
                        operand.precision,
                    ),
                )
            }
        },
    );
    let highest_precision = instruction::precision::higher_precision(
        highest_constant_precision,
        highest_non_constant_precision,
    );

    if highest_constant_precision != Precision::NotApplicable
        && highest_non_constant_precision != highest_precision
    {
        Some(highest_non_constant_precision)
    } else {
        None
    }
}

fn declare_temp_variable_if_high_precision_constant(
    state: &mut State,
    id: TypedId,
    other_operands_precision: Precision,
    transforms: &mut Vec<traverser::Transform>,
) -> TypedId {
    if let Id::Constant(constant_id) = id.id
        && instruction::precision::higher_precision(id.precision, other_operands_precision)
            != other_operands_precision
    {
        let variable_id = state.ir_meta.declare_variable(
            Name::new_temp(""),
            id.type_id,
            id.precision,
            Decorations::new_none(),
            None,
            Some(constant_id),
            VariableScope::Local,
        );
        transforms.push(traverser::Transform::DeclareVariable(variable_id));
        TypedId::from_variable_id(state.ir_meta, variable_id)
    } else {
        id
    }
}

fn declare_temp_variable_if_high_precision_constant_vec(
    state: &mut State,
    non_constant_precision: Precision,
    params: Vec<TypedId>,
    transforms: &mut Vec<traverser::Transform>,
) -> Vec<TypedId> {
    params
        .iter()
        .map(|&id| {
            declare_temp_variable_if_high_precision_constant(
                state,
                id,
                non_constant_precision,
                transforms,
            )
        })
        .collect()
}

fn declare_temp_variable_for_constant_operands(
    state: &mut State,
    id: RegisterId,
    transforms: &mut Vec<traverser::Transform>,
) {
    // If a constant is used in the instruction whose precision is higher than the other operands,
    // a temporary variable is created to hold its value so that its precision can be retained.
    // For example, if the input shader has the following:
    //
    //     const high float a = 1.0;
    //     mediump float b = 2.0;
    //     ... = a + b;
    //
    // Then `a + b` is calculated as highp.  The IR translates this to:
    //
    //     Constant c1 (float) = 1.0
    //     Variable v1 (float) = 2.0 [mediump]
    //     %result[highp] = Add c1[highp] v1
    //
    // Note that the constant precision is specified in the instruction, and not the constant
    // itself.  If naively translated to the AST, this will generate the following GLSL:
    //
    //     mediump float b = 2.0;
    //     ... = 1.0 + b;
    //
    // Which evaluates at mediump precision because the precision of `1.0` is derived as mediump
    // (from its neighboring operand `b`).  In this case, we'd need to create a temporary variable
    // for `1.0`.
    let instruction = state.ir_meta.get_instruction(id);
    let result = instruction.result;
    if result.precision == Precision::NotApplicable {
        return;
    }

    // Note: Only instructions with multiple operands need to be inspected, and only if the
    // precision of the result could depend on the constant.  This excludes for example
    // `OpCode::ConstructMatrixFromMatrix` (single operand, it would have been constant folded), or
    // `OpCode::Texture` (precision is derived from the sampler argument, the constant precision is
    // irrelevant).  Some `OpCode::Binary` and `OpCode::BuiltIn` instructions also derive their
    // precision from a specific argument, but we won't be too picky here.
    //
    // Note: `if let` expressions below are manually implementing `map()`, but that's on purpose.
    // Using `map()` leads to borrow checker errors due to the closure borrowing `state` and
    // `params` at the same time.  With the `if`, the borrow checker is able to accept the
    // implementation due to `params.clone()`.
    #[allow(clippy::manual_map)]
    let new_op = match &instruction.op {
        OpCode::ConstructVectorFromMultiple(params) => {
            if let Some(non_constant_precision) = has_constants_with_higher_precision(params) {
                Some(OpCode::ConstructVectorFromMultiple(
                    declare_temp_variable_if_high_precision_constant_vec(
                        state,
                        non_constant_precision,
                        params.clone(),
                        transforms,
                    ),
                ))
            } else {
                None
            }
        }
        OpCode::ConstructMatrixFromMultiple(params) => {
            if let Some(non_constant_precision) = has_constants_with_higher_precision(params) {
                Some(OpCode::ConstructMatrixFromMultiple(
                    declare_temp_variable_if_high_precision_constant_vec(
                        state,
                        non_constant_precision,
                        params.clone(),
                        transforms,
                    ),
                ))
            } else {
                None
            }
        }
        OpCode::ConstructArray(params) => {
            if let Some(non_constant_precision) = has_constants_with_higher_precision(params) {
                Some(OpCode::ConstructArray(declare_temp_variable_if_high_precision_constant_vec(
                    state,
                    non_constant_precision,
                    params.clone(),
                    transforms,
                )))
            } else {
                None
            }
        }
        OpCode::BuiltIn(built_in_op, params) => {
            if let Some(non_constant_precision) = has_constants_with_higher_precision(params) {
                Some(OpCode::BuiltIn(
                    *built_in_op,
                    declare_temp_variable_if_high_precision_constant_vec(
                        state,
                        non_constant_precision,
                        params.clone(),
                        transforms,
                    ),
                ))
            } else {
                None
            }
        }
        &OpCode::Binary(binary_op, lhs, rhs) => {
            if let Some(non_constant_precision) = has_constants_with_higher_precision(&[lhs, rhs]) {
                let lhs = declare_temp_variable_if_high_precision_constant(
                    state,
                    lhs,
                    non_constant_precision,
                    transforms,
                );
                let rhs = declare_temp_variable_if_high_precision_constant(
                    state,
                    rhs,
                    non_constant_precision,
                    transforms,
                );
                Some(OpCode::Binary(binary_op, lhs, rhs))
            } else {
                None
            }
        }
        _ => None,
    };

    if let Some(new_op) = new_op {
        let new_id = state.ir_meta.new_register(new_op, result.type_id, result.precision);
        state.ir_meta.replace_instruction(id, new_id.id);
    }
}

fn transform_instruction(
    state: &mut State,
    instruction: &BlockInstruction,
) -> Vec<traverser::Transform> {
    // If the instruction is:
    //
    // - a register
    // - with side effect or it's complex
    // - read multiple times
    //
    // Then a temporary variable must be created to hold the result.  For the sake of
    // simplicity, any register with a side effect is placed in a temporary, because
    // otherwise it's hard to tell when generating the AST if that statement should be
    // placed directly in the block, or whether it's used in another expression that will
    // eventually turn into a statement in the *same* block.
    //
    // TODO(http://anglebug.com/349994211): if the result of the expression with side effect is
    // never used, for example in a common `i++`, then a variable can be eliminated if
    // `has_side_effect` is `&& read_count > 0` in the `if` below, but then the read count
    // information needs to be provided to `ast::Generator` so that expressions with side effect
    // but also read_count == 0 could be placed in the block they are executed and their value
    // discarded.
    if let &BlockInstruction::Register(id) = instruction {
        let info = &state.register_info[&id];
        let cache_in_variable_if_necessary = info.has_side_effect || info.is_complex;
        let read_multiple_times = info.read_count > 1;

        if cache_in_variable_if_necessary && read_multiple_times || info.has_side_effect {
            let instruction = state.ir_meta.get_instruction(id);
            let id = instruction.result;

            // Assume the instruction is:
            //
            //     %id = ...
            //
            // This is replaced with:
            //
            //     %new_id = ...
            //               Store %new_variable %new_id
            //     %id     = Load %new_variable
            let variable_id = state.ir_meta.declare_variable(
                Name::new_temp(""),
                id.type_id,
                id.precision,
                Decorations::new_none(),
                None,
                None,
                VariableScope::Local,
            );
            let mut transforms = vec![traverser::Transform::DeclareVariable(variable_id)];

            let new_register_id = state.ir_meta.assign_new_register_to_instruction(id.id);
            declare_temp_variable_for_constant_operands(state, new_register_id, &mut transforms);

            //     %new_id = ...
            transforms
                .push(traverser::Transform::Add(BlockInstruction::new_typed(new_register_id)));

            let variable_id = TypedId::from_variable_id(state.ir_meta, variable_id);
            let new_register_id =
                TypedId::new(Id::new_register(new_register_id), id.type_id, id.precision);

            //               Store %new_variable %new_id
            traverser::add_void_instruction(
                &mut transforms,
                instruction::make!(store, state.ir_meta, variable_id, new_register_id),
            );
            //     %id     = Load %new_variable
            traverser::add_typed_instruction(
                &mut transforms,
                instruction::make_with_result_id!(load, state.ir_meta, id, variable_id),
            );

            transforms
        } else {
            let mut transforms = vec![];
            declare_temp_variable_for_constant_operands(state, id, &mut transforms);
            if !transforms.is_empty() {
                // If the instruction is rewritten (i.e. variables are added),
                // `declare_temp_variable_for_constant_operands` makes it use the same result id
                // (as `id.id`), so just keep that in the list of instructions.
                transforms.push(traverser::Transform::Keep);
            }
            transforms
        }
    } else {
        vec![]
    }
}

fn replace_merge_input_with_variable<'block>(
    state: &mut State,
    block: &'block mut Block,
) -> &'block mut Block {
    // Look at the merge block, if there is an input, it is removed and a variable is added
    // to the current block instead.
    if let Some(merge_block) = &mut block.merge_block
        && let Some(input) = merge_block.input
    {
        let variable_id = state.ir_meta.declare_variable(
            Name::new_temp(""),
            input.type_id,
            input.precision,
            Decorations::new_none(),
            None,
            None,
            VariableScope::Local,
        );

        // Add variable to the list of variables to be declared in this block.
        block.variables.push(variable_id);

        // Adjust the merge block as well as blocks that can `Merge`.
        let variable_id = TypedId::from_variable_id(state.ir_meta, variable_id);
        replace_merge_input_with_variable_in_sub_blocks(
            state,
            merge_block,
            &mut block.block1,
            &mut block.block2,
            input,
            variable_id,
        );
    }

    block
}

fn replace_merge_input_with_variable_in_sub_blocks(
    state: &mut State,
    merge_block: &mut Box<Block>,
    block1: &mut Option<Box<Block>>,
    block2: &mut Option<Box<Block>>,
    input_id: TypedRegisterId,
    variable_id: TypedId,
) {
    // Remove the merge block input and replace it with:
    //
    //     %input_id = Load %variable_id
    merge_block.prepend_instruction(instruction::make_with_result_id!(
        load,
        state.ir_meta,
        input_id,
        variable_id
    ));
    merge_block.input = None;

    // Remove the Merge(%value) from the end of each sub-block, and replace it with:
    //
    //     Store %variable_id %value
    //     Merge
    replace_merge_terminator_with_variable_store(state, block1, variable_id);
    replace_merge_terminator_with_variable_store(state, block2, variable_id);
}

fn replace_merge_terminator_with_variable_store(
    state: &mut State,
    block: &mut Option<Box<Block>>,
    variable_id: TypedId,
) {
    if let Some(block) = block {
        let block = block.get_merge_chain_last_block_mut();
        let merge_id = block.get_terminating_op().get_merge_parameter().unwrap();
        block.unterminate();

        //     Store %variable_id %value
        block.add_instruction(instruction::make!(store, state.ir_meta, variable_id, merge_id));
        //     Merge
        block.terminate(OpCode::Merge(None));
    }
}

fn transform_continue_instructions_pre_visit<'block>(
    state: &mut State,
    block: &'block mut Block,
) -> &'block mut Block {
    // Transformation 1:
    //
    // When a loop is encountered that has a continue block, that block is removed and
    // instead replicated before every `Continue` branch in the sub-blocks (that correspond
    // to this loop, and not a nested loop).  This is implemented by visiting the blocks
    // in the following way:
    //
    // - When a block terminating with `Loop` or `DoLoop` is visited, check if there is a continue
    //   block.  If so, remove it and push to `continue_stack`.  Otherwise push a None to
    //   `continue_stack`.  Loop and DoLoop are the only constructs that can be the target of a
    //   `continue` instruction, and the above enables matching continue blocks to their
    //   corresponding loops.
    // - When a block terminating with `Continue` is visited, replicate the block at the top of the
    //   stack, if any, and place it after this block.  Note that the continue block already
    //   terminates with `Continue`, and it is sufficient to change the terminating branch of this
    //   block to `NextBlock` and setting the replicated continue block as the merge block.
    // - After the `Loop` and `DoLoop` blocks are visited, pop the corresponding entry from
    //   `continue_stack`.

    // Transformation 2:
    //
    // When a do-loop is encountered, the condition block is removed, its `LoopIf
    // %condition` is changed to `if (!condition) break;`, and is replicated before every
    // `Continue` branch in the sub-blocks (that correspond to this loop).  This is
    // implemented as follows:
    //
    // - When a block terminating with `Loop`, `DoLoop` or `Switch` is visited, add an entry to
    //   `break_stack` to know which construct a nested `Break` would affect.
    // - When a block terminating with `Loop` is visited, add `None` to `condition_stack` just to
    //   indicate that `continue` applies to `Loop`, and not a `DoLoop`.
    // - When a block terminating with `DoLoop` is visited, take the condition block and place it in
    //   `condition_stack`.
    // - When a block terminating with `Continue` is visited and there is a block at the top of the
    //   `condition_stack` :
    //   - Inspect the top elements of `break_stack`, and add a variable (if not already) to all
    //     that are of `switch` type.
    //   - Replicate the condition block, and place it after this block.  The condition block is
    //     transformed such that instead of `LoopIf %condition`, it terminates with an `if
    //     (!condition) { /* set all relevant switch vars to true */ break; }` followed by a
    //     `Continue` branch.
    //  - Change the terminating branch of this block to `NextBlock` and set the replicated
    //    condition block as the merge block.
    // - After the `Switch` block is visited, check if a new variable was added while visiting the
    //   sub-blocks.  If so, add an `if (propagate_break) break;` in a block after the switch.
    // - After the `Loop`, `DoLoop` and `Switch` blocks are visited, pop the corresponding entries
    //   from `break_stack` and `condition_stack`.
    //
    // Note that the conditions for the two transformations are mutually exclusive (one
    // affects `for` loops, i.e. `Loop`, and the other do-whiles, i.e. `DoLoop`), which is
    // why they can be easily implemented in one pass.
    let mut current_block = block;

    let op = current_block.get_terminating_op();
    match op {
        OpCode::Loop => transform_continue_pre_visit_loop(state, current_block),
        OpCode::DoLoop => transform_continue_pre_visit_do_loop(state, current_block),
        OpCode::Switch(..) => transform_continue_pre_visit_switch(state, current_block),
        OpCode::Continue => {
            current_block = transform_continue_visit_continue(state, current_block);
        }
        _ => (),
    };
    current_block
}

fn transform_continue_pre_visit_loop(state: &mut State, block: &mut Block) {
    // Transformation 1: Take the loop continue block and push in `continue_stack`.
    let continue_block = block.block2.take().map(|block| *block);
    state.continue_stack.push(continue_block);

    // Transformation 2: Add an entry to `break_stack`
    state.break_stack.push(BreakInfo::new_loop());
    // Transformation 2: Add an entry to `condition_stack`
    state.condition_stack.push(None);
}

fn transform_continue_pre_visit_do_loop(state: &mut State, block: &mut Block) {
    // Transformation 1: Add an entry to `continue_stack`
    debug_assert!(block.block2.is_none());
    state.continue_stack.push(None);

    // Transformation 2: Add an entry to `break_stack`
    state.break_stack.push(BreakInfo::new_loop());
    // Transformation 2: Take the do-loop condition block and push in `condition_stack`.
    let condition_block = block.loop_condition.take().map(|block| *block);
    state.condition_stack.push(condition_block);
}

fn transform_continue_pre_visit_switch(state: &mut State, _block: &mut Block) {
    // Transformation 2: Add an entry to `break_stack`
    state.break_stack.push(BreakInfo::new_switch());
}

fn transform_continue_visit_continue<'block>(
    state: &mut State,
    block: &'block mut Block,
) -> &'block mut Block {
    let continue_block = state.continue_stack.last().unwrap().as_ref();
    let condition_block = state.condition_stack.last().unwrap().as_ref();

    let block_to_append = if let Some(continue_block) = continue_block {
        // Transformation 1: If there is a continue block at the top of the stack, the
        // `continue` corresponds to a GLSL `for` loop.  Replicate that before the
        // `Continue` instruction.
        util::duplicate_block(state.ir_meta, &mut HashMap::new(), continue_block)
    } else if let Some(condition_block) = condition_block {
        // Transformation 2: If there is a condition block at the top of the stack, the
        // `continue` corresponds to a GLSL `do-while` loop.  Add helper variables to
        // enclosing switches, if any, replicate the condition block and transform it to be
        // placed before this particular `continue`.
        let mut duplicate =
            util::duplicate_block(state.ir_meta, &mut HashMap::new(), condition_block);

        let variables_to_set = transform_continue_add_variable_to_enclosing_switch_blocks(state);
        transform_continue_adjust_condition_block(state, &mut duplicate, variables_to_set);

        duplicate
    } else {
        return block;
    };

    block.unterminate();
    block.terminate(OpCode::NextBlock);
    block.set_merge_block(block_to_append);
    block.get_merge_chain_last_block_mut()
}

fn transform_continue_add_variable_to_enclosing_switch_blocks(
    state: &mut State,
) -> Vec<VariableId> {
    let mut variables_to_set = Vec::new();

    // For each enclosing `switch` block for this `continue` instruction, add a
    // `propagate_break` variable to be used to jump out of the switch blocks and continue
    // the loop.
    for scope in state.break_stack.iter_mut().rev() {
        if !scope.is_switch {
            break;
        }

        let variable_id = match scope.propagate_break_var {
            Some(variable_id) => variable_id,
            None => {
                let variable_id = state.ir_meta.declare_variable(
                    Name::new_temp("propagate_break"),
                    TYPE_ID_BOOL,
                    Precision::NotApplicable,
                    Decorations::new_none(),
                    None,
                    Some(CONSTANT_ID_FALSE),
                    VariableScope::Local,
                );
                scope.propagate_break_var = Some(variable_id);
                variable_id
            }
        };
        variables_to_set.push(variable_id);
    }

    variables_to_set
}

fn transform_continue_adjust_condition_block(
    state: &mut State,
    block: &mut Block,
    variables_to_set: Vec<VariableId>,
) {
    let block = block.get_merge_chain_last_block_mut();

    // Since this is the condition block of a do-while loop, it must terminate with a
    // `LoopIf`.  Take the condition out of it.
    let condition = block.get_terminating_op().get_loop_condition();
    block.unterminate();

    // Create another block that only contains the `Continue` branch, and set it as the
    // merge block of the original block.
    let mut continue_block = Block::new();
    continue_block.terminate(OpCode::Continue);
    block.set_merge_block(continue_block);

    // Create a block that sets the given variables all to true, and ends in `Break`.
    let mut break_block = Block::new();
    let constant_true = TypedId::from_constant_id(CONSTANT_ID_TRUE, TYPE_ID_BOOL);
    for variable_id in variables_to_set {
        let variable_id = TypedId::from_bool_variable_id(variable_id);
        break_block.add_void_instruction(OpCode::Store(variable_id, constant_true));
    }
    break_block.terminate(OpCode::Break);

    // Terminate the current block with an `If` of the logical not of the condition, where
    // the true block is `break_block`.
    let not_instruction = instruction::make!(logical_not, state.ir_meta, condition);
    let not_condition = block.add_typed_instruction(not_instruction);
    block.terminate(OpCode::If(not_condition));
    block.set_if_true_block(break_block);
}

fn transform_continue_instructions_post_visit<'block>(
    state: &mut State,
    block: &'block mut Block,
) -> &'block mut Block {
    let mut current_block = block;

    let op = current_block.get_terminating_op();
    match op {
        OpCode::Loop | OpCode::DoLoop => transform_continue_post_visit_loop(state, current_block),
        OpCode::Switch(..) => {
            current_block = transform_continue_post_visit_switch(state, current_block);
        }
        _ => (),
    };
    current_block
}

fn transform_continue_post_visit_loop(state: &mut State, _block: &mut Block) {
    // Pop the corresponding entries from the stacks
    state.continue_stack.pop();
    state.break_stack.pop();
    state.condition_stack.pop();
}

fn transform_continue_post_visit_switch<'block>(
    state: &mut State,
    block: &'block mut Block,
) -> &'block mut Block {
    // Pop from `break_stack` only.
    let break_info = state.break_stack.pop().unwrap();

    // Check if a new variable was supposed to be added to the block.  If so, declare it,
    // and add an `if (propagate_break) break;` to the block.
    if let Some(variable_id) = break_info.propagate_break_var {
        block.variables.push(variable_id);

        // A block that only has `Break`.
        let mut break_block = Block::new();
        break_block.terminate(OpCode::Break);

        // A block that contains:
        //
        //     %value = Load %variable
        //     If %value
        let mut propagate_break_block = Block::new();
        let load_instruction =
            instruction::make!(load, state.ir_meta, TypedId::from_bool_variable_id(variable_id));
        let load_value = propagate_break_block.add_typed_instruction(load_instruction);
        propagate_break_block.terminate(OpCode::If(load_value));
        propagate_break_block.set_if_true_block(break_block);

        // Insert the new block after the one containing `Switch` (i.e. `block`) in the
        // merge chain.  This function's return value ensures that the newly added block is
        // not revisited.
        block.insert_into_merge_chain(propagate_break_block)
    } else {
        block
    }
}
