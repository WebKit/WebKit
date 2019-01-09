# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

types [
    :VirtualRegister,

    :BasicBlockLocation,
    :BoundLabel,
    :DebugHookType,
    :ErrorType,
    :GetByIdMode,
    :GetByIdModeMetadata,
    :GetPutInfo,
    :IndexingType,
    :JSCell,
    :JSGlobalLexicalEnvironment,
    :JSGlobalObject,
    :JSModuleEnvironment,
    :JSObject,
    :JSScope,
    :JSType,
    :JSValue,
    :LLIntCallLinkInfo,
    :OperandTypes,
    :ProfileTypeBytecodeFlag,
    :PropertyOffset,
    :PutByIdFlags,
    :ResolveType,
    :Structure,
    :StructureID,
    :StructureChain,
    :SymbolTable,
    :ToThisStatus,
    :TypeLocation,
    :WatchpointSet,

    :ValueProfile,
    :ValueProfileAndOperandBuffer,
    :ArithProfile,
    :ArrayProfile,
    :ArrayAllocationProfile,
    :ObjectAllocationProfile,
]

namespace :Special do
    types [ :Pointer ]
end

templates [
    :WriteBarrier,
    :WriteBarrierBase,
]


begin_section :Bytecodes,
    emit_in_h_file: true,
    emit_in_structs_file: true,
    emit_in_asm_file: true,
    emit_opcode_id_string_values_in_h_file: true,
    macro_name_component: :BYTECODE,
    asm_prefix: "llint_",
    op_prefix: "op_"

op :wide

op :enter

op :get_scope,
    args: {
        dst: VirtualRegister
    }

op :create_direct_arguments,
    args: {
        dst: VirtualRegister,
    }

op :create_scoped_arguments,
    args: {
        dst: VirtualRegister,
        scope: VirtualRegister,
    }

op :create_cloned_arguments,
    args: {
        dst: VirtualRegister,
    }

op :create_this,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        inlineCapacity: unsigned,
    },
    metadata: {
        cachedCallee: WriteBarrier[JSCell]
    }

op :get_argument,
    args: {
        dst: VirtualRegister,
        index: int,
    },
    metadata: {
        profile: ValueProfile,
    }

op :argument_count,
    args: {
        dst: VirtualRegister,
    }

op :to_this,
    args: {
        srcDst: VirtualRegister,
    },
    metadata: {
        cachedStructure: WriteBarrierBase[Structure],
        toThisStatus: ToThisStatus,
        profile: ValueProfile,
    }

op :check_tdz,
    args: {
        target: VirtualRegister,
    }

op :new_object,
    args: {
        dst: VirtualRegister,
        inlineCapacity: unsigned,
    },
    metadata: {
        objectAllocationProfile: ObjectAllocationProfile,
    }

op :new_array,
    args: {
        dst: VirtualRegister,
        argv: VirtualRegister,
        argc: unsigned,
        recommendedIndexingType: IndexingType,
    },
    metadata: {
        arrayAllocationProfile: ArrayAllocationProfile,
    }

op :new_array_with_size,
    args: {
        dst: VirtualRegister,
        length: VirtualRegister,
    },
    metadata: {
        arrayAllocationProfile: ArrayAllocationProfile,
    }

op :new_array_buffer,
    args: {
        dst: VirtualRegister,
        immutableButterfly: VirtualRegister,
        recommendedIndexingType: IndexingType
    },
    metadata: {
        arrayAllocationProfile: ArrayAllocationProfile,
    }

op :new_array_with_spread,
    args: {
        dst: VirtualRegister,
        argv: VirtualRegister,
        argc: unsigned,
        bitVector: unsigned,
    }

op :spread,
    args: {
        dst: VirtualRegister,
        argument: VirtualRegister,
    }

op :new_regexp,
    args: {
        dst: VirtualRegister,
        regexp: VirtualRegister,
    }

op :mov,
    args: {
        dst: VirtualRegister,
        src: VirtualRegister,
    }

op_group :BinaryOp,
    [
        :eq,
        :neq,
        :stricteq,
        :nstricteq,
        :less,
        :lesseq,
        :greater,
        :greatereq,
        :below,
        :beloweq,
        :mod,
        :pow,
        :lshift,
        :rshift,
        :urshift,
    ],
    args: {
        dst: VirtualRegister,
        lhs: VirtualRegister,
        rhs: VirtualRegister,
    }

op_group :ProfiledBinaryOp,
    [
        :add,
        :mul,
        :div,
        :sub,
    ],
    args: {
        dst: VirtualRegister,
        lhs: VirtualRegister,
        rhs: VirtualRegister,
        operandTypes: OperandTypes,
    },
    metadata: {
        arithProfile: ArithProfile
    },
    metadata_initializers: {
        arithProfile: :operandTypes
    }

op_group :ValueProfiledBinaryOp,
    [
        :bitand,
        :bitor,
        :bitxor,
    ],
    args: {
        dst: VirtualRegister,
        lhs: VirtualRegister,
        rhs: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile
    }

op :bitnot,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile
    }

op_group :UnaryOp,
    [
        :eq_null,
        :neq_null,
        :to_string,
        :unsigned,
        :is_empty,
        :is_undefined,
        :is_undefined_or_null,
        :is_boolean,
        :is_number,
        :is_object,
        :is_object_or_null,
        :is_function,
    ],
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
    }

op :inc,
    args: {
        srcDst: VirtualRegister,
    }

op :dec,
    args: {
        srcDst: VirtualRegister,
    }

op :to_object,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        message: unsigned,
    },
    metadata: {
        profile: ValueProfile,
    }

op :to_number,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile,
    }

op :negate,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        operandTypes: OperandTypes,
    },
    metadata: {
        arithProfile: ArithProfile,
    },
    metadata_initializers: {
        arithProfile: :operandTypes
    }

op :not,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
    }


op :identity_with_profile,
    args: {
        srcDst: VirtualRegister,
        topProfile: unsigned,
        bottomProfile: unsigned,
    }

op :overrides_has_instance,
    args: {
        dst: VirtualRegister,
        constructor: VirtualRegister,
        hasInstanceValue: VirtualRegister,
    }

op :instanceof,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
        prototype: VirtualRegister,
    }

op :instanceof_custom,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
        constructor: VirtualRegister,
        hasInstanceValue: VirtualRegister,
    }

op :typeof,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
    }

op :is_cell_with_type,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        type: JSType,
    }

op :in_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
    }

op :in_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
    }

op :get_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
    },
    metadata: {
        mode: GetByIdMode,
        hitCountForLLIntCaching: unsigned,
        modeMetadata: GetByIdModeMetadata,
        profile: ValueProfile,
    }

op :get_by_id_with_this,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: unsigned,
    },
    metadata: {
        profile: ValueProfile,
    }

op :get_by_val_with_this,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile,
    }

op :get_by_id_direct,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
    },
    metadata: {
        profile: ValueProfile, # not used in llint
        structure: StructureID,
        offset: unsigned,
    }

op :try_get_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
    },
    metadata: {
        profile: ValueProfile,
    }

op :put_by_id,
    args: {
        base: VirtualRegister,
        property: unsigned,
        value: VirtualRegister,
        flags: PutByIdFlags,
    },
    metadata: {
        oldStructure: StructureID,
        offset: unsigned,
        newStructure: StructureID,
        flags: PutByIdFlags,
        structureChain: WriteBarrierBase[StructureChain],
    },
    metadata_initializers: {
        flags: :flags
    }

op :put_by_id_with_this,
    args: {
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: unsigned,
        value: VirtualRegister,
    }

op :del_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
    }

op :get_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile,
        arrayProfile: ArrayProfile,
    }

op :put_by_val,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
    }

op :put_by_val_with_this,
    args: {
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
    }

op :put_by_val_direct,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
    }

op :del_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    }

op :put_getter_by_id,
    args: {
        base: VirtualRegister,
        property: unsigned,
        attributes: unsigned,
        accessor: VirtualRegister,
    }

op :put_setter_by_id,
    args: {
        base: VirtualRegister,
        property: unsigned,
        attributes: unsigned,
        accessor: VirtualRegister,
    }

op :put_getter_setter_by_id,
    args: {
        base: VirtualRegister,
        property: unsigned,
        attributes: unsigned,
        getter: VirtualRegister,
        setter: VirtualRegister,
    }

op :put_getter_by_val,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        attributes: unsigned,
        accessor: VirtualRegister,
    }

op :put_setter_by_val,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        attributes: unsigned,
        accessor: VirtualRegister,
    }

op :define_data_property,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
        attributes: VirtualRegister,
    }

op :define_accessor_property,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        getter: VirtualRegister,
        setter: VirtualRegister,
        attributes: VirtualRegister,
    }

op :jmp,
    args: {
        target: BoundLabel,
    }

op :jtrue,
    args: {
        condition: VirtualRegister,
        target: BoundLabel,
    }

op :jfalse,
    args: {
        condition: VirtualRegister,
        target: BoundLabel,
    }

op :jeq_null,
    args: {
        value: VirtualRegister,
        target: BoundLabel,
    }

op :jneq_null,
    args: {
        value: VirtualRegister,
        target: BoundLabel,
    }

op :jneq_ptr,
    args: {
        value: VirtualRegister,
        specialPointer: Special::Pointer,
        target: BoundLabel,
    },
    metadata: {
        hasJumped: bool,
    }

op_group :BinaryJmp,
    [
        :jeq,
        :jstricteq,
        :jneq,
        :jnstricteq,
        :jless,
        :jlesseq,
        :jgreater,
        :jgreatereq,
        :jnless,
        :jnlesseq,
        :jngreater,
        :jngreatereq,
        :jbelow,
        :jbeloweq,
    ],
    args: {
        lhs: VirtualRegister,
        rhs: VirtualRegister,
        target: BoundLabel,
    }

op :loop_hint

op_group :SwitchValue,
    [
        :switch_imm,
        :switch_char,
        :switch_string,
    ],
    args: {
        tableIndex: unsigned,
        defaultOffset: BoundLabel,
        scrutinee: VirtualRegister,
    }

op_group :NewFunction,
    [
        :new_func,
        :new_func_exp,
        :new_generator_func,
        :new_generator_func_exp,
        :new_async_func,
        :new_async_func_exp,
        :new_async_generator_func,
        :new_async_generator_func_exp,
    ],
    args: {
        dst: VirtualRegister,
        scope: VirtualRegister,
        functionDecl: unsigned,
    }

op :set_function_name,
    args: {
        function: VirtualRegister,
        name: VirtualRegister,
    }

# op_call variations
op :call,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
    },
    metadata: {
        callLinkInfo: LLIntCallLinkInfo,
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :tail_call,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
    },
    metadata: {
        callLinkInfo: LLIntCallLinkInfo,
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :call_eval,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
    },
    metadata: {
        callLinkInfo: LLIntCallLinkInfo,
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :call_varargs,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :tail_call_varargs,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :tail_call_forward_arguments,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :construct,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
    },
    metadata: {
        callLinkInfo: LLIntCallLinkInfo,
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :construct_varargs,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        profile: ValueProfile,
    }

op :ret,
    args: {
        value: VirtualRegister,
    }

op :strcat,
    args: {
        dst: VirtualRegister,
        src: VirtualRegister,
        count: int,
    }

op :to_primitive,
    args: {
        dst: VirtualRegister,
        src: VirtualRegister,
    }

op :resolve_scope,
    args: {
        dst: VirtualRegister, # offset 1
        scope: VirtualRegister, # offset 2
        var: unsigned, # offset 3
        # $begin: :private,
        resolveType: ResolveType,
        localScopeDepth: unsigned,
    },
    metadata: {
        resolveType: ResolveType, # offset 4
        localScopeDepth: unsigned, # offset 5
        _: { # offset 6
             # written during linking
             lexicalEnvironment: WriteBarrierBase[JSCell], # lexicalEnvironment && type == ModuleVar
             symbolTable: WriteBarrierBase[SymbolTable], # lexicalEnvironment && type != ModuleVar

             constantScope: WriteBarrierBase[JSScope],

             # written from the slow path
             globalLexicalEnvironment: JSGlobalLexicalEnvironment.*,
             globalObject: JSGlobalObject.*,
        },
    }

op :get_from_scope,
    args: {
        dst: VirtualRegister, # offset  1
        scope: VirtualRegister, # offset 2
        var: unsigned, # offset 3
        # $begin: :private,
        getPutInfo: GetPutInfo,
        localScopeDepth: unsigned,
        offset: unsigned,
    },
    metadata: {
        getPutInfo: GetPutInfo, # offset 4
        _: { #previously offset 5
            watchpointSet: WatchpointSet.*,
            structure: WriteBarrierBase[Structure],
        },
        operand: uintptr_t, #offset 6
        profile: ValueProfile, # offset 7
    },
    metadata_initializers: {
        getPutInfo: :getPutInfo,
        operand: :offset,
    }

op :put_to_scope,
    args: {
        scope: VirtualRegister, # offset 1
        var: unsigned, # offset 2
        value: VirtualRegister, # offset 3
        # $begin: :private,
        getPutInfo: GetPutInfo,
        symbolTableOrScopeDepth: int,
        offset: unsigned,
    },
    metadata: {
        getPutInfo: GetPutInfo, # offset 4
        _: { # offset 5
            structure: WriteBarrierBase[Structure],
            watchpointSet: WatchpointSet.*,
        },
        operand: uintptr_t, # offset 6
    },
    metadata_initializers: {
        getPutInfo: :getPutInfo,
        operand: :offset,
    }

op :get_from_arguments,
    args: {
        dst: VirtualRegister,
        arguments: VirtualRegister,
        index: unsigned,
    },
    metadata: {
        profile: ValueProfile,
    }

op :put_to_arguments,
    args: {
        arguments: VirtualRegister,
        index: unsigned,
        value: VirtualRegister,
    }

op :push_with_scope,
    args: {
        dst: VirtualRegister,
        currentScope: VirtualRegister,
        newScope: VirtualRegister,
    }

op :create_lexical_environment,
    args: {
        dst: VirtualRegister,
        scope: VirtualRegister,
        symbolTable: VirtualRegister,
        initialValue: VirtualRegister,
    }

op :get_parent_scope,
    args: {
        dst: VirtualRegister,
        scope: VirtualRegister,
    }

op :catch,
    args: {
        exception: VirtualRegister,
        thrownValue: VirtualRegister,
    },
    metadata: {
        buffer: ValueProfileAndOperandBuffer.*,
    }

op :throw,
    args: {
        value: VirtualRegister,
    }

op :throw_static_error,
    args: {
        message: VirtualRegister,
        errorType: ErrorType,
    }

op :debug,
    args: {
        debugHookType: DebugHookType,
        hasBreakpoint: bool,
    }

op :end,
    args: {
        value: VirtualRegister,
    }

op :profile_type,
    args: {
        target: VirtualRegister,
        symbolTableOrScopeDepth: int,
        flag: ProfileTypeBytecodeFlag,
        identifier?: unsigned,
        resolveType: ResolveType,
    },
    metadata: {
        typeLocation: TypeLocation.*,
    }

op :profile_control_flow,
    args: {
        textOffset: int,
    },
    metadata: {
        basicBlockLocation: BasicBlockLocation.*,
    }

op :get_enumerable_length,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
    }

op :has_indexed_property,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
    }

op :has_structure_property,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
        enumerator: VirtualRegister,
    }

op :has_generic_property,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    }

op :get_direct_pname,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
        index: VirtualRegister,
        enumerator: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile,
    }

op :get_property_enumerator,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
    }

op :enumerator_structure_pname,
    args: {
        dst: VirtualRegister,
        enumerator: VirtualRegister,
        index: VirtualRegister,
    }

op :enumerator_generic_pname,
    args: {
        dst: VirtualRegister,
        enumerator: VirtualRegister,
        index: VirtualRegister,
    }

op :to_index_string,
    args: {
        dst: VirtualRegister,
        index: VirtualRegister,
    }

op :unreachable

op :create_rest,
    args: {
        dst: VirtualRegister,
        arraySize: VirtualRegister,
        numParametersToSkip: unsigned,
    }

op :get_rest_length,
    args: {
        dst: VirtualRegister,
        numParametersToSkip: unsigned,
    }

op :yield,
    args: {
        generator: VirtualRegister,
        yieldPoint: unsigned,
        argument: VirtualRegister,
    }

op :check_traps

op :log_shadow_chicken_prologue,
    args: {
        scope: VirtualRegister,
    }

op :log_shadow_chicken_tail,
    args: {
        thisValue: VirtualRegister,
        scope: VirtualRegister,
    }

op :resolve_scope_for_hoisting_func_decl_in_eval,
    args: {
        dst: VirtualRegister,
        scope: VirtualRegister,
        property: unsigned,
    }

op :nop

op :super_sampler_begin

op :super_sampler_end

end_section :Bytecodes

begin_section :CLoopHelpers,
    emit_in_h_file: true,
    macro_name_component: :CLOOP_BYTECODE_HELPER

op :llint_entry
op :getHostCallReturnValue
op :llint_return_to_host
op :llint_vm_entry_to_javascript
op :llint_vm_entry_to_native
op :llint_cloop_did_return_from_js_1
op :llint_cloop_did_return_from_js_2
op :llint_cloop_did_return_from_js_3
op :llint_cloop_did_return_from_js_4
op :llint_cloop_did_return_from_js_5
op :llint_cloop_did_return_from_js_6
op :llint_cloop_did_return_from_js_7
op :llint_cloop_did_return_from_js_8
op :llint_cloop_did_return_from_js_9
op :llint_cloop_did_return_from_js_10
op :llint_cloop_did_return_from_js_11
op :llint_cloop_did_return_from_js_12
op :llint_cloop_did_return_from_js_13
op :llint_cloop_did_return_from_js_14
op :llint_cloop_did_return_from_js_15
op :llint_cloop_did_return_from_js_16
op :llint_cloop_did_return_from_js_17
op :llint_cloop_did_return_from_js_18
op :llint_cloop_did_return_from_js_19
op :llint_cloop_did_return_from_js_20
op :llint_cloop_did_return_from_js_21
op :llint_cloop_did_return_from_js_22
op :llint_cloop_did_return_from_js_23

end_section :CLoopHelpers

begin_section :NativeHelpers,
    emit_in_h_file: true,
    emit_in_asm_file: true,
    macro_name_component: :BYTECODE_HELPER

op :llint_program_prologue
op :llint_eval_prologue
op :llint_module_program_prologue
op :llint_function_for_call_prologue
op :llint_function_for_construct_prologue
op :llint_function_for_call_arity_check
op :llint_function_for_construct_arity_check
op :llint_generic_return_point
op :llint_throw_from_slow_path_trampoline
op :llint_throw_during_call_trampoline
op :llint_native_call_trampoline
op :llint_native_construct_trampoline
op :llint_internal_function_call_trampoline
op :llint_internal_function_construct_trampoline
op :handleUncaughtException

end_section :NativeHelpers
