// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Monomorphize functions that are called with parameters that are incompatible with the target
// language, or cause complications for future transformations:
//
// - Samplers in structs
// - Structs that have samplers
// - Partially subscripted array of array of samplers
// - Partially subscripted array of array of images
// - Atomic counters
// - Pixel local storage planes
// - image* variables with r32f formats (to emulate imageAtomicExchange)
//
// Additionally, the ESSL spec has a bug with images as function arguments, as the layout qualifiers
// needed for the image are impossible to specify.
//
// This transformation basically duplicates such functions, removes the
// sampler/image/atomic_counter parameters and uses the opaque uniforms used by the caller.
use crate::ir::*;
use crate::*;

pub struct Options {
    // Which kind of arguments are unsupported:
    pub struct_containing_samplers: bool,
    pub image: bool,
    pub atomic_counter: bool,
    pub array_of_array_of_sampler_or_image: bool,
    pub pixel_local_storage: bool,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    // Which functions to process next
    to_process: VecDeque<FunctionId>,
    // Which functions have already been processed
    processed: Vec<bool>,
    // Which functions are still alive at the end of the transformation; if all calls to a function
    // are monomorphized, the function can be deleted.
    is_live: Vec<bool>,
}

pub fn run(ir: &mut IR, options: &Options) {
    let initial_function_count = ir.meta.all_functions().len();
    let to_process = VecDeque::<FunctionId>::with_capacity(initial_function_count * 2);
    let processed = vec![false; initial_function_count * 2];
    let is_live = vec![false; initial_function_count * 2];

    let mut state = State { ir_meta: &mut ir.meta, to_process, processed, is_live };

    // Start from main() and monomorphize any function that it calls with unsupported arguments.
    // Then visit every function that main() calls, including newly monomorphized functions, and
    // recursively perform this operation.
    {
        let main_id = state.ir_meta.get_main_function_id().unwrap();
        add_to_process(&mut state, main_id);
    }

    while let Some(id) = state.to_process.pop_front() {
        if is_processed(&mut state, id) {
            continue;
        }

        let to_add = monomorphize_unsupported_calls_in_function(
            &mut state,
            ir.function_entries[id.id as usize].as_mut().unwrap(),
            options,
        );
        add_monomorphized_functions(&mut state, &mut ir.function_entries, to_add);

        state.processed[id.id as usize] = true;
    }

    // Eliminate functions that are replaced by monomorphized versions everywhere.
    (0..state.ir_meta.all_functions().len()).for_each(|id| {
        if !state.is_live[id] {
            ir.dead_code_eliminate_function(FunctionId { id: id as u32 });
        }
    });
}

// Called when a call to this function is made.  It adds the function to the queue to be processed
// later, and also marks it as live (i.e. reachable from main()).
fn add_to_process(state: &mut State, id: FunctionId) {
    if !is_processed(state, id) {
        state.to_process.push_back(id);

        let index = id.id as usize;
        if index >= state.is_live.len() {
            state.is_live.resize(index + 1, false);
        }
        state.is_live[index] = true;
    }
}

fn is_processed(state: &mut State, id: FunctionId) -> bool {
    let index = id.id as usize;
    if index >= state.processed.len() {
        state.processed.resize(index + 1, false);
    }
    state.processed[index]
}

// Unsupported arguments are always about opaque uniforms, such as images, atomic counters, samples
// in structs, etc.  When a function call is made with an unsupported argument, we need to know the
// sequence of Access* instructions that have led to the argument (which should never be `Load`ed).
struct UnsupportedArgument {
    arg_index: usize,
    // Note: the access chain is in reverse order, with [0] the one producing the argument that was
    // passed to the function, and [last] the variable itself.
    access_chain: Vec<Id>,
}

fn is_unsupported_argument(ir_meta: &IRMeta, arg: TypedId, options: &Options) -> bool {
    // The problematic types are invariably pointers; values can always be passed.
    let type_info = ir_meta.get_type(arg.type_id);
    if !type_info.is_pointer() {
        return false;
    }
    let mut type_id = type_info.get_element_type_id().unwrap();
    let mut type_info = ir_meta.get_type(type_id);
    let arg_is_array = type_info.is_array();

    // Remove array-ness to get to the base type.
    while type_info.is_array() {
        type_id = type_info.get_element_type_id().unwrap();
        type_info = ir_meta.get_type(type_id);
    }

    // If a struct containing a sampler is passed to a function, we'd like to eliminate it as a
    // function arg.
    if options.struct_containing_samplers && type_info.is_struct_containing_samplers(ir_meta) {
        return true;
    }

    // Track whether a sampler is accessed in a struct.  This is the case if the argument is of a
    // sampler (array) type, and AccessStructField is encountered.
    let is_sampler = type_info.is_sampled_image();
    let is_image = type_info.is_image();
    let is_pls = type_info.is_pixel_local_storage_plane();
    let is_atomic_counter = type_info.is_scalar_atomic_counter();
    let mut is_sampler_in_struct = false;

    // Find the variable this argument is accessing, if any.
    let mut base_variable = arg;
    loop {
        match base_variable.id {
            Id::Constant(_) => {
                return false;
            }
            Id::Variable(_) => {
                break;
            }
            Id::Register(id) => {
                let instruction = ir_meta.get_instruction(id);
                match instruction.op {
                    OpCode::AccessStructField(base_id, _) => {
                        if is_sampler {
                            // A sampler is accessed through a struct if the argument passed to the
                            // function is a sampler (array), but the access chain leading to it
                            // selects a struct field.
                            is_sampler_in_struct = true;
                        }
                        base_variable = base_id;
                    }
                    OpCode::AccessArrayElement(base_id, _) => {
                        base_variable = base_id;
                    }
                    _ => {
                        return false;
                    }
                };
            }
        }
    }

    // Now that the variable is found, see if it's unsupported.
    let type_info = ir_meta.get_type(base_variable.type_id);
    debug_assert!(type_info.is_pointer());
    let type_info = ir_meta.get_type(type_info.get_element_type_id().unwrap());
    if options.array_of_array_of_sampler_or_image {
        // Monomorphize if:
        //
        // - The opaque uniform is a sampler in a struct (which can create an array-of-array
        //   situation), and the function expects an array of samplers, or
        //
        // - The opaque uniform is an array of array of sampler or image, and it's partially
        //   subscripted (i.e. the function itself expects an array)
        if arg_is_array
            && (is_sampler_in_struct || (is_image && type_info.is_array_of_array(ir_meta)))
        {
            return true;
        }
    }
    if options.atomic_counter && is_atomic_counter {
        return true;
    }
    if options.image && is_image && !is_sampler {
        return true;
    }
    if options.pixel_local_storage && is_pls {
        return true;
    }

    false
}

fn get_access_chain(ir_meta: &IRMeta, id: TypedId) -> Vec<Id> {
    // Find the variable this argument is accessing, if any.
    let mut access_chain = vec![];
    let mut access_id = id;
    loop {
        access_chain.push(access_id.id);
        match access_id.id {
            Id::Constant(_) => {
                panic!(
                    "Internal error: Constant argument should not cause function to monomorphize"
                );
            }
            Id::Variable(_) => {
                break;
            }
            Id::Register(id) => {
                let instruction = ir_meta.get_instruction(id);
                match instruction.op {
                    OpCode::AccessStructField(base_id, _)
                    | OpCode::AccessArrayElement(base_id, _) => {
                        access_id = base_id;
                    }
                    _ => {
                        panic!(
                            "Internal error: Only opaque uniforms should cause function to monomorphize"
                        );
                    }
                };
            }
        }
    }

    access_chain
}

fn get_unsupported_arguments(
    ir_meta: &IRMeta,
    args: &[TypedId],
    options: &Options,
) -> Vec<UnsupportedArgument> {
    args.iter()
        .enumerate()
        .filter(|&(_, &id)| is_unsupported_argument(ir_meta, id, options))
        .map(&|(index, &id)| UnsupportedArgument {
            arg_index: index,
            access_chain: get_access_chain(ir_meta, id),
        })
        .collect()
}

// The monomorphization algorithm is as follows.  Take a function call argument that needs to be
// monomorphized, which always involves GLSL `uniform` variables.  In a general form, the relevant
// IR is in this form:
//
//     # Leading to function call, corresponding to `func(..., uniformVar.a[i].b[j][k], ...)`:
//     r1 = AccessStructField uniformVar Field1
//     r2 = AccessArrayElement r1 Index2
//     r3 = AccessStructField r2 Field3
//     r4 = AccessArrayElement r3 Index4
//     r5 = AccessArrayElement r4 Index5
//     ...  Call f1 ... r5 ...
//
//     # Function f1, needing monomorphization, corresponding to `param.c[l].d[m]`:
//     f1: 'func'(..., in param, ...)
//     r6 = AccessStructField param Field6
//     r7 = AccessArrayElement r6 Index7
//     r8 = AccessStructField r7 Field8
//     r9 = AccessArrayElement r8 Index9
//     r10 = Load r9
//
// We need to:
//
// * Remove `param`, such that the function directly references `uniformVar`.
// * Pass Index2, Index4 and/or Index5, if not constant, to the function
// * Remember Field1 and Field3, as well as Index2, Index4 and/or Index5 if constant to use in the
//   monomorphized version of the function.
//
// Finally, assuming `Index2` and `Index5` are not constants, but `Index4` is, the following
// changes are made to the IR:
//
//     # Leading to function call, corresponding to `func(..., i, k, ...)`:
//     # Note: r1, ..., r5 can be left for dead-code elimination later, they are harmless
//     # but eliminating them requires inspecting whether they are actually used elsewhere.
//     ...  Call f1 ... Index2, Index5 ...
//
//     # Function f1, needing monomorphization, corresponding to
//     # `uniformVar.a[i].b[j][k].c[l].d[m]`:
//     f1: 'func'(..., in index2, in index5, ...)
//     r20 = Load index2
//     r21 = Load index5
//     r30 = AccessStructField uniformVar Field1
//     r31 = AccessArrayElement r30 r20
//     r32 = AccessStructField r31 Field3
//     r33 = AccessArrayElement r32 Index4
//     r34 = AccessArrayElement r33 r21
//     r6 = AccessStructField r34 Field6
//     r7 = AccessArrayElement r6 Index7
//     r8 = AccessStructField r7 Field8
//     r9 = AccessArrayElement r8 Index9
//     r10 = Load r9
//
// Basically, this means that there are three transformations:
//
// * At the call site, the call argument is replaced with the list of non-constant indices, and the
//   call is made to a new function.
// * A preamble is generated for the function that loads those indices, and replicates the access
//   instructions at call site using constant indices or ones loaded from the function arguments.
// * The new function takes a body that is a duplicate of the original where the parameter is
//   replaced by the result of the preamble, and the preamble is prepended to the body.
struct ReplacementInfo {
    // The function parameters replacing the replaced parameter.
    params: Vec<FunctionParam>,
    // The call arguments replacing the unsupported argument.
    args: Vec<TypedId>,
    // The ID to use in the monomorphized function instead of the replaced parameter.
    mapped_param: Id,
}

fn replace_arg(state: &mut State, preamble: &mut Block, access_chain: &[Id]) -> ReplacementInfo {
    let mut params = vec![];
    let mut args = vec![];

    // Note: The access_chain is in reverse order as described in `struct UnsupportedArgument`,
    // with the uniform variable at the last index.
    let mut pointer =
        TypedId::from_variable_id(state.ir_meta, access_chain.last().unwrap().get_variable());
    access_chain.iter().rev().for_each(|id| {
        match *id {
            Id::Constant(_) => {
                panic!("Internal error: Unexpected constant pointer");
            }
            Id::Variable(id) => {
                // There is only one variable, and it's the last in the chain.
                debug_assert!(pointer.id.get_variable() == id);
            }
            Id::Register(id) => {
                // Extract the access index/field out of the instruction, create an
                // argument/parameter for non-constant indices, and replicate the instruction in
                // the preamble.
                let instruction = state.ir_meta.get_instruction(id);
                pointer = match instruction.op {
                    OpCode::AccessStructField(_, field) => preamble.add_typed_instruction(
                        instruction::struct_field(state.ir_meta, pointer, field),
                    ),
                    OpCode::AccessArrayElement(_, index) => {
                        debug_assert!(!index.id.is_variable());
                        let access_index = if index.id.is_register() {
                            // Replacement parameter to pass this index to.
                            let new_param = state.ir_meta.declare_variable(
                                Name::new_temp("index"),
                                index.type_id,
                                index.precision,
                                Decorations::new_none(),
                                None,
                                None,
                                VariableScope::FunctionParam,
                            );
                            params
                                .push(FunctionParam::new(new_param, FunctionParamDirection::Input));

                            // And the value to pass to this argument in the substituted function
                            // call.
                            args.push(index);

                            // Load from this parameter to access the pointer inside the
                            // monomorphized function.
                            preamble.add_typed_instruction(instruction::load(
                                state.ir_meta,
                                TypedId::new(
                                    Id::new_variable(new_param),
                                    index.type_id,
                                    index.precision,
                                ),
                            ))
                        } else {
                            index
                        };

                        preamble.add_typed_instruction(instruction::index(
                            state.ir_meta,
                            pointer,
                            access_index,
                        ))
                    }
                    _ => {
                        panic!("Internal error: Expected AccessStructField or AccessArrayElement");
                    }
                };
            }
        }
    });

    ReplacementInfo { params, args, mapped_param: pointer.id }
}

struct MonomorphizedFunction {
    // Original function to monomorphize
    original_function_id: FunctionId,
    // The ID of the new function
    new_function_id: FunctionId,
    // The map of replaced parameters to the ID to use instead (i.e. the uniform variable itself,
    // or a part of it accessed in the preamble)
    param_map: HashMap<VariableId, Id>,
    // The preamble to prepend to the duplicated function body
    preamble: Block,
}

fn duplicate_function_params(
    ir_meta: &mut IRMeta,
    replacement_params: &mut Vec<FunctionParam>,
    original_params: &[FunctionParam],
    param_map: &mut HashMap<VariableId, Id>,
) {
    replacement_params.extend(original_params.iter().map(|param| {
        FunctionParam::new(
            {
                let replacement = util::duplicate_variable(ir_meta, param.variable_id);
                param_map.insert(param.variable_id, Id::new_variable(replacement));
                replacement
            },
            param.direction,
        )
    }));
}

fn prepare_to_monomorphize(
    state: &mut State,
    function_id: FunctionId,
    result: Option<TypedRegisterId>,
    args: Vec<TypedId>,
    unsupported_args: Vec<UnsupportedArgument>,
    transforms: &mut Vec<traverser::Transform>,
) -> MonomorphizedFunction {
    let mut preamble = Block::new();
    let function_params = state.ir_meta.get_function(function_id).params.clone();

    let mut unreplaced_index = 0;
    let mut replacement_args = vec![];
    let mut replacement_params = vec![];
    let mut param_map = HashMap::new();

    // Loop over the arguments, building the function call's replacement arguments and the preamble
    // to the monomorphized function.  The parameters to replace in the monomorphized function are
    // gathered as well.
    for arg in unsupported_args.iter() {
        if unreplaced_index < arg.arg_index {
            replacement_args.extend_from_slice(&args[unreplaced_index..arg.arg_index]);
            duplicate_function_params(
                state.ir_meta,
                &mut replacement_params,
                &function_params[unreplaced_index..arg.arg_index],
                &mut param_map,
            );
        }

        let replacement = replace_arg(state, &mut preamble, &arg.access_chain);
        replacement_args.extend_from_slice(&replacement.args);
        replacement_params.extend_from_slice(&replacement.params);
        param_map.insert(function_params[arg.arg_index].variable_id, replacement.mapped_param);

        unreplaced_index = arg.arg_index + 1;
    }
    if unreplaced_index < args.len() {
        replacement_args.extend_from_slice(&args[unreplaced_index..]);
        duplicate_function_params(
            state.ir_meta,
            &mut replacement_params,
            &function_params[unreplaced_index..],
            &mut param_map,
        );
    }

    // Declare a new function.  The body of the function will be created after iteration over this
    // function is finished (as it accesses other elements of ir.function_entries and appends to
    // it).
    let function = state.ir_meta.get_function(function_id);
    debug_assert!((function.return_type_id == TYPE_ID_VOID) == result.is_none());
    let new_function = Function::new(
        function.name.name,
        replacement_params,
        function.return_type_id,
        function.return_precision,
        function.return_decorations.clone(),
    );
    let new_function_id = state.ir_meta.add_function(new_function);
    add_to_process(state, new_function_id);

    // Replace the function call with a call to this new function.  The result, if any, uses the
    // same ID as the original.
    if let Some(result) = result {
        traverser::add_typed_instruction(
            transforms,
            instruction::make_with_result_id!(
                call,
                state.ir_meta,
                result,
                new_function_id,
                replacement_args
            ),
        );
    } else {
        traverser::add_void_instruction(
            transforms,
            instruction::make!(call, state.ir_meta, new_function_id, replacement_args),
        );
    }

    MonomorphizedFunction {
        original_function_id: function_id,
        new_function_id,
        param_map,
        preamble,
    }
}

#[allow(unused_variables)]
fn transform_call(
    state: &mut State,
    instruction: &BlockInstruction,
    options: &Options,
    to_add: &mut Vec<MonomorphizedFunction>,
) -> Vec<traverser::Transform> {
    let (op, result) = instruction.get_op_and_result(state.ir_meta);
    if let OpCode::Call(function_id, args) = op {
        let unsupported_args = get_unsupported_arguments(state.ir_meta, args, options);
        if unsupported_args.is_empty() {
            // Nothing to do.  Just visit this function later.
            add_to_process(state, *function_id);
            return vec![];
        }

        // Satisfy the borrow checker as we're about to mutate `state`.
        let args = args.clone();
        let mut transforms = vec![];
        to_add.push(prepare_to_monomorphize(
            state,
            *function_id,
            result,
            args,
            unsupported_args,
            &mut transforms,
        ));
        transforms
    } else {
        vec![]
    }
}

fn monomorphize_unsupported_calls_in_function(
    state: &mut State,
    entry: &mut Block,
    options: &Options,
) -> Vec<MonomorphizedFunction> {
    let mut to_add = vec![];
    traverser::transformer::for_each_block(
        &mut (state, &mut to_add),
        entry,
        &|state, block| {
            traverser::transformer::transform_block(
                state,
                block,
                &|(state, to_add), instruction| transform_call(state, instruction, options, to_add),
            )
        },
        &|_, block| block,
    );
    to_add
}

fn add_monomorphized_functions(
    state: &mut State,
    function_entries: &mut Vec<Option<Block>>,
    to_add: Vec<MonomorphizedFunction>,
) {
    to_add.into_iter().for_each(|mut to_add| {
        // Duplicate the original function's block:
        let mut new_body = util::duplicate_block(
            state.ir_meta,
            &mut to_add.param_map,
            function_entries[to_add.original_function_id.id as usize].as_ref().unwrap(),
        );
        // Add the preamble:
        if !to_add.preamble.instructions.is_empty() {
            new_body.prepend_code(to_add.preamble);
        }
        // Set new body as the function's body.
        let index = to_add.new_function_id.id as usize;
        if index >= function_entries.len() {
            function_entries.resize_with(index + 1, || None);
        }
        debug_assert!(function_entries[index].is_none());
        function_entries[index] = Some(new_body);
    });
}
