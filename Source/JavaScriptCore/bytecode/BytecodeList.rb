# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
    :ECMAMode,
    :ErrorTypeWithExtension,
    :GetByIdMode,
    :GetByIdModeMetadata,
    :GetByValHistory,
    :GetPutInfo,
    :IndexingType,
    :IterationModeMetadata,
    :JSCell,
    :JSGlobalLexicalEnvironment,
    :JSGlobalObject,
    :JSModuleEnvironment,
    :JSObject,
    :JSScope,
    :JSType,
    :JSValue,
    :LLIntCallLinkInfo,
    :ResultType,
    :OperandTypes,
    :ProfileTypeBytecodeFlag,
    :PropertyOffset,
    :PutByIdFlags,
    :PutByValFlags,
    :ResolveType,
    :Structure,
    :StructureID,
    :StructureChain,
    :SymbolTable,
    :SymbolTableOrScopeDepth,
    :ToThisStatus,
    :TypeLocation,
    :WasmBoundLabel,
    :WatchpointSet,

    :ValueProfile,
    :ValueProfileAndVirtualRegisterBuffer,
    :UnaryArithProfile,
    :BinaryArithProfile,
    :ArrayProfile,
    :ArrayAllocationProfile,
    :ObjectAllocationProfile,
]

templates [
    :WriteBarrier,
    :WriteBarrierBase,
]


begin_section :Bytecode,
    emit_in_h_file: true,
    emit_in_structs_file: true,
    emit_in_asm_file: true,
    emit_opcode_id_string_values_in_h_file: true,
    macro_name_component: :BYTECODE,
    asm_prefix: "llint_",
    op_prefix: "op_"

op :wide16
op :wide32

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

op :create_arguments_butterfly,
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

op :create_promise,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        isInternalPromise: bool,
    },
    metadata: {
        cachedCallee: WriteBarrier[JSCell]
    }

op :new_promise,
    args: {
        dst: VirtualRegister,
        isInternalPromise: bool,
    }

op :new_generator,
    args: {
        dst: VirtualRegister,
    }

op_group :CreateInternalFieldObjectOp,
    [
        :create_generator,
        :create_async_generator,
    ],
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
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
        ecmaMode: ECMAMode,
    },
    metadata: {
        cachedStructureID: StructureID,
        toThisStatus: ToThisStatus,
        profile: ValueProfile,
    }

op :check_tdz,
    args: {
        targetVirtualRegister: VirtualRegister,
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
        arithProfile: BinaryArithProfile
    }

op_group :ValueProfiledBinaryOp,
    [
        :bitand,
        :bitor,
        :bitxor,
        :lshift,
        :rshift,
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
        :typeof_is_undefined,
        :is_undefined_or_null,
        :is_boolean,
        :is_number,
        :is_big_int,
        :is_object,
        :is_object_or_null,
        :is_function,
        :is_constructor,
    ],
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
    }

op_group :UnaryInPlaceProfiledOp,
    [
        :inc,
        :dec,
    ],
    args: {
        srcDst: VirtualRegister,
    },
    metadata: {
        arithProfile: UnaryArithProfile
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

op_group :ValueProfiledUnaryOp,
    [
        :to_number,
        :to_numeric,
    ],
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
        resultType: ResultType,
    },
    metadata: {
        arithProfile: UnaryArithProfile,
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
        structureID: StructureID,
        offset: unsigned,
    }

op :get_prototype_of,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile,
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
        oldStructureID: StructureID,
        offset: unsigned,
        newStructureID: StructureID,
        structureChain: WriteBarrierBase[StructureChain],
    }

op :put_by_id_with_this,
    args: {
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: unsigned,
        value: VirtualRegister,
        ecmaMode: ECMAMode,
    }

op :del_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
        ecmaMode: ECMAMode,
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
        seenIdentifiers: GetByValHistory,
    }

op :get_private_name,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    },
    metadata: {
        profile: ValueProfile,
        structureID: StructureID,
        offset: unsigned,
        property: WriteBarrier[JSCell],
    }

op :put_by_val,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
        ecmaMode: ECMAMode,
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
        ecmaMode: ECMAMode,
    }

op :put_by_val_direct,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
        flags: PutByValFlags,
    },
    metadata: {
        arrayProfile: ArrayProfile,
    }

op :del_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
        ecmaMode: ECMAMode,
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
        targetLabel: BoundLabel,
    }

op :jtrue,
    args: {
        condition: VirtualRegister,
        targetLabel: BoundLabel,
    }

op :jfalse,
    args: {
        condition: VirtualRegister,
        targetLabel: BoundLabel,
    }

op :jeq_null,
    args: {
        value: VirtualRegister,
        targetLabel: BoundLabel,
    }

op :jneq_null,
    args: {
        value: VirtualRegister,
        targetLabel: BoundLabel,
    }

op :jundefined_or_null,
    args: {
        value: VirtualRegister,
        targetLabel: BoundLabel,
    }

op :jnundefined_or_null,
    args: {
        value: VirtualRegister,
        targetLabel: BoundLabel,
    }

op :jneq_ptr,
    args: {
        value: VirtualRegister,
        specialPointer: VirtualRegister,
        targetLabel: BoundLabel,
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
        targetLabel: BoundLabel,
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
        profile: ValueProfile,
    }

op :call_eval,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
        ecmaMode: ECMAMode,
    },
    metadata: {
        callLinkInfo: LLIntCallLinkInfo,
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
    },
    tmps: {
        argCountIncludingThis: unsigned,
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
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
    },
    tmps: {
        argCountIncludingThis: unsigned
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
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
    },
    tmps: {
        argCountIncludingThis: unsigned
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
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

op :to_property_key,
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
        _0: { # offset 5
            localScopeDepth: unsigned,
            globalLexicalBindingEpoch: unsigned,
        },
        _1: { # offset 6
             # written during linking
             lexicalEnvironment: WriteBarrierBase[JSCell], # lexicalEnvironment && type == ModuleVar
             symbolTable: WriteBarrierBase[SymbolTable], # lexicalEnvironment && type != ModuleVar

             constantScope: WriteBarrierBase[JSScope],

             # written from the slow path
             globalLexicalEnvironment: WriteBarrierBase[JSGlobalLexicalEnvironment],
             globalObject: WriteBarrierBase[JSGlobalObject],
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
        symbolTableOrScopeDepth: SymbolTableOrScopeDepth,
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

op :create_generator_frame_environment,
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
        buffer: ValueProfileAndVirtualRegisterBuffer.*,
    }

op :throw,
    args: {
        value: VirtualRegister,
    }

op :throw_static_error,
    args: {
        message: VirtualRegister,
        errorType: ErrorTypeWithExtension,
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
        targetVirtualRegister: VirtualRegister,
        symbolTableOrScopeDepth: SymbolTableOrScopeDepth,
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

op :has_own_structure_property,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
        enumerator: VirtualRegister,
    }

op :in_structure_property,
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

# Semantically, this is iterator = symbolIterator.@call(iterable); next = iterator.next;
# where symbolIterator the result of iterable[Symbol.iterator] (which is done in a different bytecode).
# For builtin iterators, however, this has special behavior where next becomes the empty value, which
# indicates that we are in a known iteration mode to op_iterator_next.
op :iterator_open,
    args: {
        iterator: VirtualRegister,
        next: VirtualRegister,
        symbolIterator: VirtualRegister,
        iterable: VirtualRegister,
        stackOffset: unsigned,
    },
    metadata: {
        iterationMetadata: IterationModeMetadata,
        iterableProfile: ValueProfile,
        callLinkInfo: LLIntCallLinkInfo,
        iteratorProfile: ValueProfile,
        modeMetadata: GetByIdModeMetadata,
        nextProfile: ValueProfile,
    },
    checkpoints: {
        symbolCall: nil,
        getNext: nil,
    }

# Semantically, this is nextResult = next.@call(iterator); done = nextResult.done; value = done ? undefined : nextResult.value;
op :iterator_next,
    args: {
        done: VirtualRegister,
        value: VirtualRegister,
        iterable: VirtualRegister,
        next: VirtualRegister,
        iterator: VirtualRegister,
        stackOffset: unsigned,
    },
    metadata: {
        iterationMetadata: IterationModeMetadata,
        iterableProfile: ArrayProfile,
        callLinkInfo: LLIntCallLinkInfo,
        nextResultProfile: ValueProfile,
        doneModeMetadata: GetByIdModeMetadata,
        doneProfile: ValueProfile,
        valueModeMetadata: GetByIdModeMetadata,
        valueProfile: ValueProfile,
    },
    tmps: {
        nextResult: JSValue,
    },
    checkpoints: {
        computeNext: nil,
        getDone: nil,
        getValue: nil,
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

op :get_internal_field,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        index: unsigned,
    },
    metadata: {
        profile: ValueProfile,
    }

op :put_internal_field,
    args: {
        base: VirtualRegister,
        index: unsigned,
        value: VirtualRegister,
    }

op :nop

op :super_sampler_begin

op :super_sampler_end

end_section :Bytecode

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
op :llint_cloop_did_return_from_js_24
op :llint_cloop_did_return_from_js_25
op :llint_cloop_did_return_from_js_26
op :llint_cloop_did_return_from_js_27
op :llint_cloop_did_return_from_js_28
op :llint_cloop_did_return_from_js_29
op :llint_cloop_did_return_from_js_30
op :llint_cloop_did_return_from_js_31
op :llint_cloop_did_return_from_js_32
op :llint_cloop_did_return_from_js_33
op :llint_cloop_did_return_from_js_34
op :llint_cloop_did_return_from_js_35
op :llint_cloop_did_return_from_js_36
op :llint_cloop_did_return_from_js_37
op :llint_cloop_did_return_from_js_38
op :llint_cloop_did_return_from_js_39
op :llint_cloop_did_return_from_js_40
op :llint_cloop_did_return_from_js_41
op :llint_cloop_did_return_from_js_42
op :llint_cloop_did_return_from_js_43
op :llint_cloop_did_return_from_js_44
op :llint_cloop_did_return_from_js_45
op :llint_cloop_did_return_from_js_46

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
op :checkpoint_osr_exit_from_inlined_call_trampoline
op :checkpoint_osr_exit_trampoline
op :fuzzer_return_early_from_loop_hint
op :handleUncaughtException
op :op_call_return_location
op :op_construct_return_location
op :op_call_varargs_slow_return_location
op :op_construct_varargs_slow_return_location
op :op_get_by_id_return_location
op :op_get_by_val_return_location
op :op_put_by_id_return_location
op :op_put_by_val_return_location
op :op_iterator_open_return_location
op :op_iterator_next_return_location
op :wasm_function_prologue
op :wasm_function_prologue_no_tls

end_section :NativeHelpers

begin_section :Wasm,
    emit_in_h_file: true,
    emit_in_structs_file: true,
    macro_name_component: :WASM,
    op_prefix: "wasm_"

autogenerate_wasm_opcodes

# Helpers

op :throw_from_slow_path_trampoline

# FIXME: Wasm and JS LLInt should share common opcodes
# https://bugs.webkit.org/show_bug.cgi?id=203656

op :wide16
op :wide32

op :enter
op :nop
op :loop_hint

op :mov,
    args: {
        dst: VirtualRegister,
        src: VirtualRegister,
    }

op_group :ConditionalJump,
    [
        :jtrue,
        :jfalse,
    ],
    args: {
        condition: VirtualRegister,
        targetLabel: WasmBoundLabel,
    }

op :jmp,
    args: {
        targetLabel: WasmBoundLabel,
    }

op :ret

op :switch,
    args: {
        scrutinee: VirtualRegister,
        tableIndex: unsigned,
    }

# Wasm specific bytecodes

op :unreachable
op :ret_void

op :drop_keep,
    args: {
        startOffset: unsigned,
        dropCount: unsigned,
        keepCount: unsigned,
    }

op :ref_is_null,
    args: {
        dst: VirtualRegister,
        ref: VirtualRegister,
    }

op :ref_func,
    args: {
        dst: VirtualRegister,
        functionIndex: unsigned,
    }

op :get_global,
    args: {
        dst: VirtualRegister,
        globalIndex: unsigned,
    }

op :set_global,
    args: {
        globalIndex: unsigned,
        value: VirtualRegister,
    }

op :set_global_ref,
    args: {
        globalIndex: unsigned,
        value: VirtualRegister,
    }

op :get_global_portable_binding,
    args: {
        dst: VirtualRegister,
        globalIndex: unsigned,
    }

op :set_global_portable_binding,
    args: {
        globalIndex: unsigned,
        value: VirtualRegister,
    }

op :set_global_ref_portable_binding,
    args: {
        globalIndex: unsigned,
        value: VirtualRegister,
    }

op :table_get,
    args: {
        dst: VirtualRegister,
        index: VirtualRegister,
        tableIndex: unsigned,
    }

op :table_set,
    args: {
        index: VirtualRegister,
        value: VirtualRegister,
        tableIndex: unsigned,
    }

op :table_size,
    args: {
        dst: VirtualRegister,
        tableIndex: unsigned,
    }

op :table_grow,
    args: {
        dst: VirtualRegister,
        fill: VirtualRegister,
        size: VirtualRegister,
        tableIndex: unsigned,
    }

op :table_fill,
    args: {
        offset: VirtualRegister,
        fill: VirtualRegister,
        size: VirtualRegister,
        tableIndex: unsigned,
    }

op :call,
    args: {
        functionIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
    }

op :call_no_tls,
    args: {
        functionIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
    }

op :call_indirect,
    args: {
        functionIndex: VirtualRegister,
        signatureIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
        tableIndex: unsigned,
    }

op :call_indirect_no_tls,
    args: {
        functionIndex: VirtualRegister,
        signatureIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
        tableIndex: unsigned,
    }

op :current_memory,
    args: {
        dst: VirtualRegister,
    }

op :grow_memory,
    args: {
        dst: VirtualRegister,
        delta: VirtualRegister
    }

op :select,
    args: {
        dst: VirtualRegister,
        condition: VirtualRegister,
        nonZero: VirtualRegister,
        zero: VirtualRegister,
    }

op_group :Load,
    [
        :load8_u,
        :load16_u,
        :load32_u,
        :load64_u,
        :i32_load8_s,
        :i64_load8_s,
        :i32_load16_s,
        :i64_load16_s,
        :i64_load32_s,
    ],
    args: {
        dst: VirtualRegister,
        pointer: VirtualRegister,
        offset: unsigned,
    }

op_group :Store,
    [
        :store8,
        :store16,
        :store32,
        :store64,
    ],
    args: {
        pointer: VirtualRegister,
        value: VirtualRegister,
        offset: unsigned,
    }

end_section :Wasm
