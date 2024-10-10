# Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
    :DataOnlyCallLinkInfo,
    :DebugHookType,
    :ECMAMode,
    :ErrorTypeWithExtension,
    :EnumeratorMetadata,
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
    :ResultType,
    :OperandTypes,
    :PrivateFieldPutKind,
    :ProfileTypeBytecodeFlag,
    :PropertyOffset,
    :PutByIdFlags,
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

    :ValueProfileAndVirtualRegisterBuffer,
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
    op_prefix: "op_",
    preserve_order: true

# Ops with checkpoint must come first
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
        callLinkInfo: DataOnlyCallLinkInfo,
    },
    tmps: {
        argCountIncludingThis: unsigned
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
    }

op :call_varargs,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
    },
    tmps: {
        argCountIncludingThis: unsigned,
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
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
        nextResultValueProfile: unsigned,
        doneValueProfile: unsigned,
        valueValueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        doneModeMetadata: GetByIdModeMetadata,
        valueModeMetadata: GetByIdModeMetadata,
        iterableProfile: ArrayProfile,
        iterationMetadata: IterationModeMetadata,
    },
    tmps: {
        nextResult: JSValue,
    },
    checkpoints: {
        computeNext: nil,
        getDone: nil,
        getValue: nil,
    }

op :construct_varargs,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
    },
    tmps: {
        argCountIncludingThis: unsigned
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
    }

op :super_construct_varargs,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        thisValue?: VirtualRegister,
        arguments?: VirtualRegister,
        firstFree: VirtualRegister,
        firstVarArg: int,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        cachedCallee: WriteBarrier[JSCell],
    },
    tmps: {
        argCountIncludingThis: unsigned
    },
    checkpoints: {
        determiningArgCount: nil,
        makeCall: nil,
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
        iterableValueProfile: unsigned,
        iteratorValueProfile: unsigned,
        nextValueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        modeMetadata: GetByIdModeMetadata,
        arrayProfile: ArrayProfile,
        iterationMetadata: IterationModeMetadata,
    },
    checkpoints: {
        symbolCall: nil,
        getNext: nil,
    }

# Semantically, this is dst = value instanceof constructor.
op :instanceof,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
        constructor: VirtualRegister,
        hasInstanceValueProfile: unsigned,
        prototypeValueProfile: unsigned,
    },
    metadata: {
        hasInstanceModeMetadata: GetByIdModeMetadata,
        prototypeModeMetadata: GetByIdModeMetadata,
    },
    checkpoints: {
        getHasInstance: nil,
        getPrototype: nil,
        instanceof: nil,
    }

# Opcodes with metadata come next, in decreasing order of metadata alignment requirements
# Alignment: 8
op :set_private_brand, args: {
        base: VirtualRegister,
        brand: VirtualRegister,
    },
    metadata: {
        oldStructureID: StructureID,
        newStructureID: StructureID,
        brand: WriteBarrier[JSCell],
    }

op :check_private_brand,
    args: {
        base: VirtualRegister,
        brand: VirtualRegister,
    },
    metadata: {
        structureID: StructureID,
        brand: WriteBarrier[JSCell],
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

op :construct,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
    }

op :super_construct,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        cachedCallee: WriteBarrier[JSCell],
    }

op :tail_call,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        arrayProfile: ArrayProfile,
    }

op :call_direct_eval,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
        thisValue: VirtualRegister,
        scope: VirtualRegister,
        lexicallyScopedFeatures: unsigned,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
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
        callLinkInfo: DataOnlyCallLinkInfo,
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

op :create_promise,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        isInternalPromise: bool,
    },
    metadata: {
        cachedCallee: WriteBarrier[JSCell]
    }

op :catch,
    args: {
        exception: VirtualRegister,
        thrownValue: VirtualRegister,
    },
    metadata: {
        buffer: ValueProfileAndVirtualRegisterBuffer.*,
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

op :get_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
        valueProfile: unsigned,
    },
    metadata: {
        modeMetadata: GetByIdModeMetadata,
    }

op :get_length,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        valueProfile: unsigned,
    },
    metadata: {
        modeMetadata: GetByIdModeMetadata,
        arrayProfile: ArrayProfile,
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

op :new_array_with_species,
    args: {
        dst: VirtualRegister,
        length: VirtualRegister,
        array: VirtualRegister,
        valueProfile: unsigned,
    },
    metadata: {
        arrayAllocationProfile: ArrayAllocationProfile,
        arrayProfile: ArrayProfile,
    }

# op_call variations
op :call,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
        valueProfile: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        arrayProfile: ArrayProfile,
    }

op :call_ignore_result,
    args: {
        callee: VirtualRegister,
        argc: unsigned,
        argv: unsigned,
    },
    metadata: {
        callLinkInfo: DataOnlyCallLinkInfo,
        arrayProfile: ArrayProfile,
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
        valueProfile: unsigned, # offset 7
    },
    metadata: {
        getPutInfo: GetPutInfo, # offset 4
        _: { #previously offset 5
            watchpointSet: WatchpointSet.*,
            structure: WriteBarrierBase[Structure],
        },
        operand: uintptr_t, #offset 6
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

op :create_this,
    args: {
        dst: VirtualRegister,
        callee: VirtualRegister,
        inlineCapacity: unsigned,
    },
    metadata: {
        cachedCallee: WriteBarrier[JSCell]
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

op :get_by_val_with_this,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: VirtualRegister,
        valueProfile: unsigned,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        seenIdentifiers: GetByValHistory,
    }

op :get_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
        valueProfile: unsigned,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        seenIdentifiers: GetByValHistory,
    }

op :put_private_name,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
        putKind: PrivateFieldPutKind,
    },
    metadata: {
        property: WriteBarrier[JSCell],
        oldStructureID: StructureID,
        offset: unsigned,
        newStructureID: StructureID,
    }

op :get_private_name,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
        valueProfile: unsigned,
    },
    metadata: {
        structureID: StructureID,
        offset: unsigned,
        property: WriteBarrier[JSCell],
    }

# Alignment: 4
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

op :put_by_val_direct,
    args: {
        base: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
        ecmaMode: ECMAMode,
    },
    metadata: {
        arrayProfile: ArrayProfile,
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

op :enumerator_next,
    args: {
        # out
        propertyName: VirtualRegister,
        # in/out
        mode: VirtualRegister, # Will always be a JS UInt32 representing a JSForInMode.
        index: VirtualRegister, # Gets reset to zero every time mode changes.
        # in
        base: VirtualRegister,
        enumerator: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        enumeratorMetadata: EnumeratorMetadata,
    }

op :enumerator_in_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        mode: VirtualRegister,
        propertyName: VirtualRegister,
        index: VirtualRegister,
        enumerator: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        enumeratorMetadata: EnumeratorMetadata,
    }

op :enumerator_has_own_property,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        mode: VirtualRegister,
        propertyName: VirtualRegister,
        index: VirtualRegister,
        enumerator: VirtualRegister,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        enumeratorMetadata: EnumeratorMetadata,
    }

op :enumerator_put_by_val,
    args: {
        base: VirtualRegister,
        mode: VirtualRegister,
        propertyName: VirtualRegister,
        index: VirtualRegister,
        enumerator: VirtualRegister,
        value: VirtualRegister,
        ecmaMode: ECMAMode,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        enumeratorMetadata: EnumeratorMetadata,
    }

op :to_this,
    args: {
        srcDst: VirtualRegister,
        ecmaMode: ECMAMode,
        valueProfile: unsigned,
    },
    metadata: {
        cachedStructureID: StructureID,
        toThisStatus: ToThisStatus,
    }

op :enumerator_get_by_val,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        mode: VirtualRegister,
        propertyName: VirtualRegister,
        index: VirtualRegister,
        enumerator: VirtualRegister,
        valueProfile: unsigned,
    },
    metadata: {
        arrayProfile: ArrayProfile,
        enumeratorMetadata: EnumeratorMetadata,
    }

op :get_by_id_direct,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
        valueProfile: unsigned, # not used in llint
    },
    metadata: {
        structureID: StructureID,
        offset: unsigned,
    }

op :try_get_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
        valueProfile: unsigned,
    },
    metadata: {
        structureID: StructureID,
        offset: unsigned,
    }

# Alignment: 1
op :jneq_ptr,
    args: {
        value: VirtualRegister,
        specialPointer: VirtualRegister,
        targetLabel: BoundLabel,
    },
    metadata: {
        hasJumped: bool,
    }

# Opcodes without metadata are last
op :get_argument,
    args: {
        dst: VirtualRegister,
        index: int,
        valueProfile: unsigned,
    }

op :get_from_arguments,
    args: {
        dst: VirtualRegister,
        arguments: VirtualRegister,
        index: unsigned,
        valueProfile: unsigned,
    }

op :get_prototype_of,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
        valueProfile: unsigned,
    }

op :get_internal_field,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        index: unsigned,
        valueProfile: unsigned,
    }

op :get_by_id_with_this,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: unsigned,
        valueProfile: unsigned,
    }

op :to_object,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        message: unsigned,
        valueProfile: unsigned,
    }

op :in_by_id,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: unsigned,
    }

op :has_private_name,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        property: VirtualRegister,
    }

op :has_private_brand,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
        brand: VirtualRegister,
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

op :put_by_val_with_this,
    args: {
        base: VirtualRegister,
        thisValue: VirtualRegister,
        property: VirtualRegister,
        value: VirtualRegister,
        ecmaMode: ECMAMode,
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

op :jeq_ptr,
    args: {
        value: VirtualRegister,
        specialPointer: VirtualRegister,
        targetLabel: BoundLabel,
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

op :to_property_key_or_number,
    args: {
        dst: VirtualRegister,
        src: VirtualRegister,
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

op :get_property_enumerator,
    args: {
        dst: VirtualRegister,
        base: VirtualRegister,
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

op :put_internal_field,
    args: {
        base: VirtualRegister,
        index: unsigned,
        value: VirtualRegister,
    }

op :nop

op :super_sampler_begin

op :wide16

op :super_sampler_end

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

op :new_promise,
    args: {
        dst: VirtualRegister,
        isInternalPromise: bool,
    }

op :new_generator,
    args: {
        dst: VirtualRegister,
    }

op :argument_count,
    args: {
        dst: VirtualRegister,
    }

op :check_tdz,
    args: {
        targetVirtualRegister: VirtualRegister,
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

op_group :ProfiledBinaryOpWithOperandTypes,
    [
        :add,
        :mul,
        :div,
        :sub,
        :bitand,
        :bitor,
        :bitxor,
    ],
    args: {
        dst: VirtualRegister,
        lhs: VirtualRegister,
        rhs: VirtualRegister,
        profileIndex: unsigned,
        operandTypes: OperandTypes,
    }

op_group :ProfiledBinaryOp,
    [
        :lshift,
        :rshift,
    ],
    args: {
        dst: VirtualRegister,
        lhs: VirtualRegister,
        rhs: VirtualRegister,
        profileIndex: unsigned,
    }

op_group :UnaryOp,
    [
        :eq_null,
        :neq_null,
        :to_string,
        :unsigned,
        :is_empty,
        :typeof_is_undefined,
        :typeof_is_object,
        :typeof_is_function,
        :is_undefined_or_null,
        :is_boolean,
        :is_number,
        :is_big_int,
        :is_object,
        :is_callable,
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
        profileIndex: unsigned,
    }

op :negate,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        profileIndex: unsigned,
        resultType: ResultType,
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

op :has_structure_with_flags,
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        flags: unsigned,
    }

op_group :ProfiledUnaryOp,
    [
        :to_number,
        :to_numeric,
        :bitnot,
    ],
    args: {
        dst: VirtualRegister,
        operand: VirtualRegister,
        profileIndex: unsigned,
    }

end_section :Bytecode

begin_section :CLoopHelpers,
    emit_in_h_file: true,
    macro_name_component: :CLOOP_BYTECODE_HELPER

op :llint_entry
op :llint_vm_entry_to_javascript
op :llint_vm_entry_to_native

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
op :llint_default_call_trampoline
op :llint_virtual_call_trampoline
op :llint_virtual_construct_trampoline
op :llint_virtual_tail_call_trampoline
op :llint_polymorphic_normal_call_trampoline
op :llint_polymorphic_closure_call_trampoline
op :checkpoint_osr_exit_from_inlined_call_trampoline
op :checkpoint_osr_exit_trampoline
op :normal_osr_exit_trampoline
op :fuzzer_return_early_from_loop_hint
op :loop_osr_entry_gate
op :llint_get_host_call_return_value
op :llint_handle_uncaught_exception
op :op_call_return_location
op :op_call_ignore_result_return_location
op :op_construct_return_location
op :op_super_construct_return_location
op :op_call_varargs_return_location
op :op_construct_varargs_return_location
op :op_super_construct_varargs_return_location
op :op_get_by_id_return_location
op :op_get_by_id_direct_return_location
op :op_get_length_return_location
op :op_get_by_val_return_location
op :op_put_by_id_return_location
op :op_put_by_val_return_location
op :op_put_by_val_direct_return_location
op :op_in_by_id_return_location
op :op_in_by_val_return_location
op :op_enumerator_get_by_val_return_location
op :op_enumerator_put_by_val_return_location
op :op_enumerator_in_by_val_return_location
op :op_iterator_open_return_location
op :op_iterator_next_return_location
op :op_call_direct_eval_slow_return_location
op :wasm_function_prologue_trampoline
op :wasm_function_prologue
op :wasm_function_prologue_simd_trampoline
op :wasm_function_prologue_simd
op :js_to_wasm_wrapper_entry
op :wasm_to_wasm_wrapper_entry
op :wasm_to_js_wrapper_entry
op :ipint_trampoline

op :js_trampoline_op_call
op :js_trampoline_op_call_ignore_result
op :js_trampoline_op_construct
op :js_trampoline_op_super_construct
op :js_trampoline_op_call_varargs
op :js_trampoline_op_construct_varargs
op :js_trampoline_op_super_construct_varargs
op :js_trampoline_op_iterator_next
op :js_trampoline_op_iterator_open
op :js_trampoline_op_call_direct_eval_slow
op :js_trampoline_llint_function_for_call_arity_check_untag
op :js_trampoline_llint_function_for_call_arity_check_tag
op :js_trampoline_llint_function_for_construct_arity_check_untag
op :js_trampoline_llint_function_for_construct_arity_check_tag
op :wasm_trampoline_wasm_call
op :wasm_trampoline_wasm_call_indirect
op :wasm_trampoline_wasm_call_ref
op :wasm_trampoline_wasm_tail_call
op :wasm_trampoline_wasm_tail_call_indirect
op :wasm_trampoline_wasm_tail_call_ref

end_section :NativeHelpers

begin_section :CLoopReturnHelpers,
    emit_in_h_file: true,
    macro_name_component: :CLOOP_RETURN_HELPER

op :llint_return_to_host
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

end_section :CLoopReturnHelpers

begin_section :Wasm,
    emit_in_h_file: true,
    emit_in_structs_file: true,
    macro_name_component: :WASM,
    op_prefix: "wasm_"

autogenerate_wasm_opcodes

# Helpers

op :throw_from_slow_path_trampoline
op :throw_from_fault_handler_trampoline_reg_instance

op :call_return_location
op :call_indirect_return_location
op :call_ref_return_location

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

op :ref_as_non_null,
    args: {
        dst: VirtualRegister,
        ref: VirtualRegister,
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

op :table_init,
    args: {
        dstOffset: VirtualRegister,
        srcOffset: VirtualRegister,
        length: VirtualRegister,
        elementIndex: unsigned,
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

op :tail_call,
    args: {
        functionIndex: unsigned,
        stackOffset: unsigned,
        numberOfCalleeStackArgs: unsigned,
        numberOfCallerStackArgs: unsigned,
    }

op :call_indirect,
    args: {
        functionIndex: VirtualRegister,
        typeIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
        tableIndex: unsigned,
    }

op :tail_call_indirect,
    args: {
        functionIndex: VirtualRegister,
        signatureIndex: unsigned,
        stackOffset: unsigned,
        numberOfCalleeStackArgs: unsigned,
        numberOfCallerStackArgs: unsigned,
        tableIndex: unsigned,
    }

op :call_ref,
    args: {
        functionReference: VirtualRegister,
        typeIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
    }

op :tail_call_ref,
    args: {
        functionReference: VirtualRegister,
        typeIndex: unsigned,
        stackOffset: unsigned,
        numberOfCalleeStackArgs: unsigned,
        numberOfCallerStackArgs: unsigned,
    }

op :call_builtin,
    args: {
        builtinIndex: unsigned,
        stackOffset: unsigned,
        numberOfStackArgs: unsigned,
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

op_group :AtomicBinaryRMW,
    [
        "add",
        "sub",
        "and",
        "or",
        "xor",
        "xchg",
    ].flat_map {|op|
        [
            "i64_atomic_rmw_#{op}",
            "i64_atomic_rmw8_#{op}_u",
            "i64_atomic_rmw16_#{op}_u",
            "i64_atomic_rmw32_#{op}_u",
        ]
    }.map {|op| op.to_sym },
    args: {
        dst: VirtualRegister,
        pointer: VirtualRegister,
        offset: unsigned,
        value: VirtualRegister,
    }

op_group :AtomicCompareExchange,
    [
        :i64_atomic_rmw_cmpxchg,
        :i64_atomic_rmw8_cmpxchg_u,
        :i64_atomic_rmw16_cmpxchg_u,
        :i64_atomic_rmw32_cmpxchg_u,
    ],
    args: {
        dst: VirtualRegister,
        pointer: VirtualRegister,
        offset: unsigned,
        expected: VirtualRegister,
        value: VirtualRegister,
    }

op_group :AtomicWait,
    [
        :memory_atomic_wait32,
        :memory_atomic_wait64,
    ],
    args: {
        dst: VirtualRegister,
        pointer: VirtualRegister,
        offset: unsigned,
        value: VirtualRegister,
        timeout: VirtualRegister,
    }

op :memory_atomic_notify,
    args: {
        dst: VirtualRegister,
        pointer: VirtualRegister,
        offset: unsigned,
        count: VirtualRegister,
    }

op :atomic_fence,
    args: {
    }

op :throw,
    args: {
        exceptionIndex: unsigned,
        firstValue: VirtualRegister,
    }

op :rethrow,
    args: {
        exception: VirtualRegister,
    }

op :throw_ref,
    args: {
        exception: VirtualRegister,
    }

op_group :Catch,
    [
        :catch,
    ],
    args: {
        exceptionIndex: unsigned,
        exception: VirtualRegister,
        argumentCount: unsigned,
        startOffset: unsigned,
    }

op_group :CatchAll,
    [
        :catch_all,
    ],
    args: {
        exception: VirtualRegister,
    }

op_group :TryTableCatch,
    [
        :try_table_catch,
        :try_table_catchref,
        :try_table_catchall,
        :try_table_catchallref,
    ],
    args: {
        kind: unsigned,
        exceptionIndex: unsigned,
        exception: VirtualRegister,
        argumentCount: unsigned,
        startOffset: unsigned,
    }

op :ref_i31,
    args: {
        dst: VirtualRegister,
        value: VirtualRegister,
    }

op :i31_get,
    args: {
        dst: VirtualRegister,
        ref: VirtualRegister,
        isSigned: bool,
    }

op :array_new,
    args: {
        dst: VirtualRegister,
        size: VirtualRegister,
        value: VirtualRegister,
        typeIndex: unsigned,
        arrayNewKind: uint8_t,
    }

op :array_get,
    args: {
        dst: VirtualRegister,
        arrayref: VirtualRegister,
        index: VirtualRegister,
        typeIndex: unsigned,
        arrayGetKind: unsigned,
    }

op :array_set,
    args: {
        arrayref: VirtualRegister,
        index: VirtualRegister,
        value: VirtualRegister,
        typeIndex: unsigned,
    }

op :array_len,
    args: {
        dst: VirtualRegister,
        arrayref: VirtualRegister,
    }

op :array_fill,
    args: {
        arrayref: VirtualRegister,
        offset: VirtualRegister,
        value: VirtualRegister,
        size: VirtualRegister,
        typeIndex: unsigned,
    }

op :struct_new,
    args: {
        dst: VirtualRegister,
        typeIndex: unsigned,
        useDefault: bool,
        firstValue: VirtualRegister,
    }

op :struct_get,
    args: {
        dst: VirtualRegister,
        structReference: VirtualRegister,
        fieldIndex: unsigned,
        structGetKind: unsigned,
    }

op :struct_set,
    args: {
        structReference: VirtualRegister,
        fieldIndex: unsigned,
        value: VirtualRegister,
    }

op :extern_convert_any,
    args: {
        dst: VirtualRegister,
        reference: VirtualRegister,
    }

end_section :Wasm
