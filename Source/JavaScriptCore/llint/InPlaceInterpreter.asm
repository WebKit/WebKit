#
# IPInt: the WASM in-place interpreter
# DISCLAIMER: not tested on x86 yet (as of 05 Jul 2023); IPInt may break *very* badly.
#
# docs by Daniel Liu <daniel_liu4@apple.com / danlliu@umich.edu>; 2023 intern project
#
# 0. OfflineASM:
# --------------
#
# For a crash course on OfflineASM, check out LowLevelInterpreter.asm.
#
# 1. Code Structure
# -----------------
#
# IPInt is designed to start up quickly and interpret WASM code efficiently. To optimize for speed, we utilize a jump
# table, using the opcode's first byte as an offset. This jump table is set up in _ipint_setup. 
# For more complex opcodes (ex. SIMD), we define additional jump tables that utilize further bytes as indices.
#
# 2. Setting Up
# -------------
#
# Before we can execute WebAssembly, we have to handle the call frame that is given to us. This is handled in _ipint_entry.
# We start by saving registers to the stack as per the system calling convention. Then, we have IPInt specific logic:
#
# 2.1. Locals
# -----------
#
# To ensure that we are able to access local variables quickly, we allocate a section of the stack to store local variables.
# We allocate 8 bytes for each local variable on the stack.
#
# Additionally, we need to load the parameters to the function into local variables. As per the calling convention, arguments
# are passed via registers, and then on the stack if all argument registers have been exhausted. Thus, we need to handle those
# cases. We keep track of the number of arguments in IPIntCallee, allowing us to know exactly where to load arguments from.
#
# Finally, we set the value of the `PL` (pointer to locals) register to the position of the first local. This allows us to quickly
# index into locals.
#
# 2.2. Bytecode and Metadata
# --------------------------
#
# The final step before executing is to load the bytecode to execute, as well as the metadata. For an explanation of why we use
# metadata in IPInt, check out WasmIPIntGenerator.cpp. We load these into registers `PB` (pointer to bytecode) and `PM` (pointer
# to metadata). Additionally, registers `PC` (program counter) and `MC` (metadata counter) are set to 0.
#
# 3. Executing WebAssembly
# ------------------------
#
# WebAssembly execution revolves around a stack machine, which we run on the program stack. We work with the constraint
# that the stack must be 16B aligned by ensuring that pushes and pops are always 16B. This makes certain opcodes (ex. drop)
# much easier as well.
#
# For each instruction, we align its assembly to a 256B boundary. Thus, we can take (address of instruction 0) + opcode * 256
# to find the exact point where we need to jump for each opcode without any dependent loads.
#
# 4. Returning
# ------------
#
# To return values to the caller, IPInt uses the standard WebAssembly calling convention. Return values are passed in the
# system return registers, and on the stack if not possible. After this, we perform cleanup logic to reset the stack to its
# original state, and return to the caller.
#

#################################
# Register and Size Definitions #
#################################

# PC = t4
const MC = t5  # Metadata counter (index into metadata)
const PL = t6  # Pointer to locals (index into locals)
const PM = metadataTable

if ARM64 or ARM64E
    const IB = t7  # instruction base
end

# TODO: SIMD support, since locals will need double the space. Can we do it only sometimes?
# May just need to write metadata that rewrites offsets. May be worth the space savings.
# Actually, what if we just use the same thing but have a SIMD section separately allocated that
# is "pointed" to by the 8B entries on the stack? Easier and we only need to allocate SIMD when we need
# instead of blowing up the stack. Argument copying a little trickier though.

const PtrSize = constexpr (sizeof(void*))
const MachineRegisterSize = constexpr (sizeof(CPURegister))
const SlotSize = constexpr (sizeof(Register))
const LocalSize = SlotSize
const StackValueSize = 16

if X86_64 or ARM64 or ARM64E or RISCV64
    const wasmInstance = csr0
    const memoryBase = csr3
    const boundsCheckingSize = csr4
elsif ARMv7
    const wasmInstance = csr0
    const memoryBase = invalidGPR
    const boundsCheckingSize = invalidGPR
else
end

const WasmCodeBlock = CallerFrame - constexpr Wasm::numberOfIPIntCalleeSaveRegisters * SlotSize - MachineRegisterSize

##########
# Macros #
##########

# Callee Save

macro saveIPIntRegisters()
    subp 2*CalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq wasmInstance, PB, -16[cfr]
        storep PM, -24[cfr]
    elsif X86_64 or RISCV64
        storep PB, -0x8[cfr]
        storep wasmInstance, -0x10[cfr]
        storep PM, -0x18[cfr]
    else
    end
end

macro restoreIPIntRegisters()
    if ARM64 or ARM64E
        loadpairq -16[cfr], wasmInstance, PB
        loadp -24[cfr], PM
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], PB
        loadp -0x10[cfr], wasmInstance
        loadp -0x18[cfr], PM
    else
    end
    addp 2*CalleeSaveSpaceStackAligned, sp
end

# Get IPIntCallee object at startup

macro getIPIntCallee()
    loadp Callee[cfr], ws0
if JSVALUE64
    andp ~(constexpr JSValue::WasmTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    storep ws0, WasmCodeBlock[cfr]
end

# Tail-call dispatch

macro advancePC(amount)
    addq amount, PC
end

macro advancePCByReg(amount)
    addq amount, PC
end

macro advanceMC(amount)
    addq amount, MC
end

macro advanceMCByReg(amount)
    addq amount, MC
end

macro nextIPIntInstruction()
    # Consistency check
    # move MC, t0
    # andq 7, t0
    # bqeq t0, 0, .fine
    # break
# .fine:
    loadb [PB, PC, 1], t0
if ARM64 or ARM64E
    # x7 = IB
    # x0 = opcode
    emit "add x0, x7, x0, lsl #8"
    emit "br x0"
elsif X86_64
    lshiftq 8, t0
    leap (_ipint_unreachable), t1
    addq t1, t0
    emit "jmp *(%eax)"
else
    break
end
end

# Stack operations
# Every value on the stack is always 16 bytes! This makes life easy.

macro pushQuad(reg)
    if ARM64 or ARM64E
        push reg, reg
    else
        push reg
    end
end

macro pushQuadPair(reg1, reg2)
    push reg1, reg2
end

macro popQuad(reg, scratch)
    if ARM64 or ARM64E
        pop reg, scratch
    else
        pop reg
    end
end

# Pushes ft0 because macros
macro pushFPR()
    if ARM64 or ARM64E
        emit "str q0, [sp, #-16]!"
    else
        emit "sub $16, %esp"
        emit "movdqu %xmm0, (%esp)"
    end
end

macro pushFPR1()
    if ARM64 or ARM64E
        emit "str q1, [sp, #-16]!"
    else
        emit "sub $16, %esp"
        emit "movdqu %xmm1, (%esp)"
    end
end

macro popFPR()
    if ARM64 or ARM64E
        # We'll just drop the entire q0 register in here
        # to keep stack aligned to 16
        # We'll never actually use q0 as a whole for FP,
        # since we only work with f32 (s0) or f64 (d0)
        emit "ldr q0, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm0"
        emit "add $16, %esp"
    end
end

macro popFPR1()
    if ARM64 or ARM64E
        emit "ldr q1, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm1"
        emit "add $16, %esp"
    end
end

# Typed push/pop to make code pretty

macro pushInt32(reg)
    pushQuad(reg)
end

macro popInt32(reg, scratch)
    popQuad(reg, scratch)
end

macro pushInt64(reg)
    pushQuad(reg)
end

macro popInt64(reg, scratch)
    popQuad(reg, scratch)
end

macro pushFloat32FT0()
    pushFPR()
end

macro pushFloat32FT1()
    pushFPR1()
end

macro popFloat32FT0()
    popFPR()
end

macro popFloat32FT1()
    popFPR1()
end

macro pushFloat64FT0()
    pushFPR()
end

macro pushFloat64FT1()
    pushFPR1()
end

macro popFloat64FT0()
    popFPR()
end

macro popFloat64FT1()
    popFPR1()
end

# Instruction labels

macro alignment()
if ARM64 or ARM64E
    # fill with brk instructions
    emit ".balignl 256, 0xd4388e20"
elsif X86_64
    # fill with int 3 instructions
    emit ".balign 256, 0xcc"
end
end

macro instructionLabel(instrname)
    alignment()
    unalignedglobal _ipint%instrname%_validate
    _ipint%instrname%:
    _ipint%instrname%_validate:
end

macro unimplementedInstruction(instrname)
    alignment()
    instructionLabel(instrname)
    break
end

macro reservedOpcode(opcode)
    alignment()
    break
end

# Memory

macro ipintReloadMemory()
    if ARM64 or ARM64E
        loadpairq Wasm::Instance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    else
        loadp Wasm::Instance::m_cachedMemory[wasmInstance], memoryBase
        loadp Wasm::Instance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    cagedPrimitiveMayBeNull(memoryBase, boundsCheckingSize, t2, t3)
end

# Operation Calls

macro operationCall(fn)
    move wasmInstance, a0
    push PC, MC
    push PL, ws0
    fn()
    pop ws0, PL
    pop MC, PC
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
end

macro operationCallMayThrow(fn)
    move wasmInstance, a0
    push PC, MC
    push PL, ws0
    fn()
    bqneq r0, 1, .continuation
    storei r1, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
.continuation:
    pop ws0, PL
    pop MC, PC
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
end

# Exception handling

macro ipintException(exception)
    # move PL, sp
    # loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], PM
    # mulq SlotSize, PM
    # addq PM, sp
    # restoreCallerPCAndCFR()
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    restoreIPIntRegisters()
    jmp _wasm_throw_from_slow_path_trampoline
end

########################
# In-Place Interpreter #
########################

global _ipint_entry
_ipint_entry:
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64)
    preserveCallerPCAndCFR()
    saveIPIntRegisters()
    storep wasmInstance, CodeBlock[cfr]
    getIPIntCallee()

    # Allocate space for locals
    loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], csr0
    mulq LocalSize, csr0
    subq csr0, sp

    if ARM64 or ARM64E
        storepairq t0, t1, 0x00[sp]
        storepairq t2, t3, 0x10[sp]
        storepairq t4, t5, 0x20[sp]
        storepairq t6, t7, 0x30[sp]
        storepaird fa0, fa1, 0x40[sp]
        storepaird fa2, fa3, 0x50[sp]
    elsif JSVALUE64
        storeq t0, 0x00[sp]
        storeq t1, 0x08[sp]
        storeq t2, 0x10[sp]
        storeq t3, 0x18[sp]
        storeq t4, 0x20[sp]
        storeq t5, 0x28[sp]
        stored fa0, 0x40[sp]
        stored fa1, 0x48[sp]
        stored fa2, 0x50[sp]
        stored fa3, 0x58[sp]
    else
        storeq t0, 0x00[sp]
        storeq t1, 0x08[sp]
        storeq t2, 0x10[sp]
        storeq t3, 0x18[sp]
        stored fa0, 0x40[sp]
        stored fa1, 0x48[sp]
    end
    move sp, PL

    # Copy over arguments on stack
    # csr0 = argument index
    # csr1 = total arguments
    # csr2 = tmp
    # csr3 = stack index
    # csr4 = first argument offset
    move 0, csr0
    loadi Wasm::IPIntCallee::m_numArgumentsOnStack[ws0], csr1
    move 16, csr3
    leap FirstArgumentOffset[cfr], csr4

.argloop:
    bqeq csr0, csr1, .endargs
    # Load from stack
    loadq [csr4, csr0, LocalSize], csr2
    storeq csr2, 0x80[PL, csr0, LocalSize]
    addq 1, csr0
    jmp .argloop

.endargs:
    # Zero out everything else
    loadi Wasm::IPIntCallee::m_numArgumentsOnStack[ws0], csr0
    addq 16, csr0
    loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], csr1
.localLoop:
    bieq csr0, csr1, .endlocals
    storeq 0, [PL, csr0, LocalSize]
    addq 1, csr0
    jmp .localLoop
.endlocals:

    loadp CodeBlock[cfr], wasmInstance
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
    loadp Wasm::IPIntCallee::m_bytecode[ws0], PB
    move 0, PC
    loadp Wasm::IPIntCallee::m_metadata[ws0], PM
    move 0, MC
    # Load memory
    ipintReloadMemory()

    nextIPIntInstruction()

.ipint_exit:
    # Clean up locals
    # Don't overwrite the return registers
    # Will use PM as a temp because we don't want to use the actual temps.
    move PL, sp
    loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], PM
    mulq SlotSize, PM
    addq PM, sp

    restoreIPIntRegisters()
    restoreCallerPCAndCFR()
    ret
else
    ret
end

if WEBASSEMBLY and (ARM64 or ARM64E or X86_64)
# Put all instructions after this `if`, or 32 bit will fail to build.

    #############################
    # 0x00 - 0x11: control flow #
    #############################

instructionLabel(_unreachable)
    # unreachable
    ipintException(Unreachable)

instructionLabel(_nop)
    # nop
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_block)
    # block
    loadq [PM, MC], PC
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_loop)
    # loop
    loadq [PM, MC], PC
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_if)
    # if
    popInt32(t0, t1)
    bqneq 0, t0, .ipint_if_taken
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()
.ipint_if_taken:
    # Skip LEB128
    loadq 8[PM, MC], PC
    advanceMC(16)
    nextIPIntInstruction()

instructionLabel(_else)
    # else
    # Counterintuitively, we only run this instruction if the if
    # clause is TAKEN. This is used to branch to the end of the
    # block.
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

reservedOpcode(0x06)
reservedOpcode(0x07)
reservedOpcode(0x08)
reservedOpcode(0x09)
reservedOpcode(0x0a)

instructionLabel(_end)
    loadi Wasm::IPIntCallee::m_bytecodeLength[ws0], t0
    subq 1, t0
    bqeq PC, t0, .ipint_end_ret
    advancePC(1)
    nextIPIntInstruction()
.ipint_end_ret:
    # Get number of returns
    loadh [PM, MC], t1
    move MC, t0
    addq 2, t0
    addq MC, t1
.ipint_ret_loop:
    bqeq t0, t1, .ipint_ret_loop_end
    loadb [PM, t0], t2
    bqeq t2, 0, .ipint_ret_loop_end
    addq 1, t0
    bqgteq t2, 6, .ipint_ret_stack
    bqeq t2, 5, .ipint_ret_fpr
    popQuad(r0, r1)
    jmp .ipint_ret_loop
.ipint_ret_fpr:
    popFPR()
    jmp .ipint_ret_loop
.ipint_ret_stack:
    # TODO
    break
.ipint_ret_loop_end:
    jmp .ipint_exit

instructionLabel(_br)
    # br
    # number to pop
    loadh 8[PM, MC], t0
    # number to keep
    loadh 10[PM, MC], t1

    # ex. pop 3 and keep 2
    #
    # +4 +3 +2 +1 sp
    # a  b  c  d  e
    # d  e
    #
    # [sp + k + numToPop] = [sp + k] for k in numToKeep-1 -> 0
    move t0, t2
    lshiftq 4, t2
    leap [sp, t2], t2

.ipint_br_poploop:
    bqeq t1, 0, .ipint_br_popend
    subq 1, t1
    move t1, t3
    lshiftq 4, t3
    loadq [sp, t3], t0
    storeq t0, [t2, t3]
    loadq 8[sp, t3], t0
    storeq t0, 8[t2, t3]
    jmp .ipint_br_poploop
.ipint_br_popend:
    loadh 8[PM, MC], t0
    lshiftq 4, t0
    leap [sp, t0], sp
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_br_if)
    # pop i32
    popInt32(t0, t2)
    bineq t0, 0, _ipint_br
    loadi 12[PM, MC], PC
    advanceMC(16)
    nextIPIntInstruction()

instructionLabel(_br_table)
    # br_table
    popInt32(t0, t2)
    loadq [PM, MC], t1
    advanceMC(8)
    biaeq t0, t1, .ipint_br_table_maxout
    lshiftq 4, t0
    addq t0, MC
    jmp _ipint_br
.ipint_br_table_maxout:
    subq 1, t1
    lshiftq 4, t1
    addq t1, MC
    jmp _ipint_br

instructionLabel(_return)
    # ret
    loadi Wasm::IPIntCallee::m_bytecodeLength[ws0], PC
    loadi Wasm::IPIntCallee::m_returnMetadata[ws0], MC
    subq 1, PC
    # This is guaranteed going to an end instruction, so skip
    # dispatch and end of program check for speed
    jmp .ipint_end_ret

instructionLabel(_call)
    # call
    jmp _ipint_call_impl

instructionLabel(_call_indirect)
    # Get ref
    # Load pre-computed values from metadata
    popInt32(t0, t1)
    push PC, MC # a4
    move t0, a2
    leap [PM, MC], a3
    move wasmInstance, a0
    move cfr, a1
    operationCall(macro() cCall4(_ipint_extern_call_indirect) end)
    pop MC, PC
    btpz r1, .ipint_call_indirect_throw

    advanceMC(8)

    jmp .ipint_call_common
.ipint_call_indirect_throw:
    jmp _wasm_throw_from_slow_path_trampoline

reservedOpcode(0x12)
reservedOpcode(0x13)
reservedOpcode(0x14)
reservedOpcode(0x15)
reservedOpcode(0x16)
reservedOpcode(0x17)
reservedOpcode(0x18)
reservedOpcode(0x19)

instructionLabel(_drop)
    addq StackValueSize, sp
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_select)
    popInt32(t0, t2)
    bieq t0, 0, .ipint_select_val2
    addq 16, sp
    advancePC(1)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_select_val2:
    popQuad(t1, t2)
    popQuad(t0, t2)
    pushQuad(t1)
    advancePC(1)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_select_t)
    popInt32(t0, t2)
    bieq t0, 0, .ipint_select_t_val2
    addq 16, sp
    loadi [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_select_t_val2:
    popQuad(t1, t2)
    popQuad(t0, t3)
    pushQuadPair(t2, t1)
    loadi [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

reservedOpcode(0x1d)
reservedOpcode(0x1e)
reservedOpcode(0x1f)

    ###################################
    # 0x20 - 0x26: get and set values #
    ###################################

instructionLabel(_local_get)
    # local.get
    # Load pre-computed index from metadata
    loadi [PM, MC], t0
    # Index into locals
    loadq [PL, t0, LocalSize],t0
    # Push to stack
    pushQuad(t0)

    loadi 4[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_local_set)
    # local.set
    # Load pre-computed index from metadata
    loadi [PM, MC], t0
    # Pop from stack
    popQuad(t1, t2)
    # Store to locals
    storeq t1, [PL, t0, LocalSize]

    loadi 4[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_local_tee)
    # local.tee
    loadi [PM, MC], t0
    loadq [sp], t1
    storeq t1, [PL, t0, LocalSize]

    loadi 4[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(16)
    nextIPIntInstruction()

instructionLabel(_global_get)
    # Load pre-computed index from metadata
    loadh 6[PM, MC], t2
    loadi [PM, MC], t1
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    lshiftp 1, t1
    loadq [t0, t1, 8], t0
    bieq t2, 0, .ipint_global_get_embedded
    loadq [t0], t0
.ipint_global_get_embedded:
    pushQuad(t0)

    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_global_set)
    # b7 = 1 => ref, use slowpath
    loadb 7[PM, MC], t0
    bineq t0, 0, .ipint_global_set_refpath
    # b6 = 1 => portable
    loadb 6[PM, MC], t2
    # get global addr
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    # get value to store
    popQuad(t3, t1)
    # get index
    loadi [PM, MC], t1
    lshiftp 1, t1
    bieq t2, 0, .ipint_global_set_embedded
    # portable: dereference then set
    loadq [t0, t1, 8], t0
    storeq t3, [t0]
    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_global_set_embedded:
    # embedded: set directly
    storeq t3, [t0, t1, 8]
    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

.ipint_global_set_refpath:
    loadi [PM, MC], a1
    # Pop from stack
    popQuad(a2, t3)
    operationCall(macro() cCall3(_ipint_extern_set_global_64) end)

    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_table_get)
    # Load pre-computed index from metadata
    loadi [PM, MC], a1
    popInt32(a2, t3)

    operationCallMayThrow(macro() cCall3(_ipint_extern_table_get) end)

    pushQuad(t0)

    loadi 4[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_table_set)
    # Load pre-computed index from metadata
    loadi [PM, MC], a1
    popQuad(a3, t0)
    popInt32(a2, t0)
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_set) end)

    loadi 4[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

reservedOpcode(0x27)

macro ipintCheckMemoryBound(mem, size)
    leap size - 1[mem], t2
    bpb t2, boundsCheckingSize, .continuation
    break
    ipintException(OutOfBoundsMemoryAccess)
.continuation:
    nop
end

instructionLabel(_i32_load_mem)
    # i32.load
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt32(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_load_mem)
    # i32.load
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 8)
    # load memory location
    loadq [memoryBase, t0], t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_f32_load_mem)
    # f32.load
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    loadf [memoryBase, t0], ft0
    pushFloat32FT0()

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
    
instructionLabel(_f64_load_mem)
    # f64.load
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 8)
    # load memory location
    loadd [memoryBase, t0], ft0
    pushFloat64FT0()

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
    

instructionLabel(_i32_load8s_mem)
    # i32.load8_s
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2i t1, t1
    pushInt32(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i32_load8u_mem)
    # i32.load8_u
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt32(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i32_load16s_mem)
    # i32.load16_s
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2i t1, t1
    pushInt32(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i32_load16u_mem)
    # i32.load16_u
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt32(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()


instructionLabel(_i64_load8s_mem)
    # i64.load8_s
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2q t1, t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_load8u_mem)
    # i64.load8_u
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_load16s_mem)
    # i64.load16_s
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2q t1, t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_load16u_mem)
    # i64.load16_u
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_load32s_mem)
    # i64.load32_s
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    sxi2q t1, t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_load32u_mem)
    # i64.load8_s
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt64(t1)

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()


instructionLabel(_i32_store_mem)
    # i32.store
    # pop data
    popInt32(t1, t2)
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    storei t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_store_mem)
    # i64.store
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 8)
    # load memory location
    storeq t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_f32_store_mem)
    # f32.store
    # pop data
    popFloat32FT0()
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    storef ft0, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_f64_store_mem)
    # f64.store
    # pop data
    popFloat64FT0()
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 8)
    # load memory location
    stored ft0, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i32_store8_mem)
    # i32.store8
    # pop data
    popInt32(t1, t2)
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 1)
    # load memory location
    storeb t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i32_store16_mem)
    # i32.store16
    # pop data
    popInt32(t1, t2)
    # pop index
    popInt32(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 2)
    # load memory location
    storeh t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
    
instructionLabel(_i64_store8_mem)
    # i64.store8
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 1)
    # load memory location
    storeb t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_store16_mem)
    # i64.store16
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 2)
    # load memory location
    storeh t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_store32_mem)
    # i64.store32
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi [PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, 4)
    # load memory location
    storei t1, [memoryBase, t0]

    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()


instructionLabel(_memory_size)
    operationCall(macro() cCall2(_ipint_extern_current_memory) end)
    pushInt32(r0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_memory_grow)
    popInt32(a1, t2)
    operationCall(macro() cCall2(_ipint_extern_memory_grow) end)
    pushInt32(r0)
    ipintReloadMemory()
    advancePC(2)
    nextIPIntInstruction()

    ################################
    # 0x41 - 0x44: constant values #
    ################################

instructionLabel(_i32_const)
    # i32.const
    # Load pre-computed value from metadata
    loadi [PM, MC], t0
    # Push to stack
    pushInt32(t0)
    loadi 4[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_i64_const)
    # i64.const
    # Load pre-computed value from metadata
    loadq [PM, MC], t0
    # Push to stack
    pushInt64(t0)
    loadq 8[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(16)
    nextIPIntInstruction()

instructionLabel(_f32_const)
    # f32.const
    # Load pre-computed value from metadata
    loadf 1[PB, PC], ft0
    pushFloat32FT0()

    advancePC(5)
    nextIPIntInstruction()

instructionLabel(_f64_const)
    # f64.const
    # Load pre-computed value from metadata
    loadd 1[PB, PC], ft0
    pushFloat64FT0()

    advancePC(9)
    nextIPIntInstruction()

    ###############################
    # 0x45 - 0x4f: i32 comparison #
    ###############################

instructionLabel(_i32_eqz)
    # i32.eqz
    popInt32(t0, t2)
    cieq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_eq)
    # i32.eq
    popInt32(t1, t2)
    popInt32(t0, t2)
    cieq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ne)
    # i32.ne
    popInt32(t1, t2)
    popInt32(t0, t2)
    cineq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_lt_s)
    # i32.lt_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cilt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_lt_u)
    # i32.lt_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cib t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_gt_s)
    # i32.gt_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cigt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_gt_u)
    # i32.gt_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cia t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_le_s)
    # i32.le_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cilteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_le_u)
    # i32.le_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cibeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ge_s)
    # i32.ge_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cigteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ge_u)
    # i32.ge_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    ciaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x50 - 0x5a: i64 comparison #
    ###############################

instructionLabel(_i64_eqz)
    # i64.eqz
    popInt64(t0, t2)
    cqeq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_eq)
    # i64.eq
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ne)
    # i64.ne
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqneq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_lt_s)
    # i64.lt_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqlt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_lt_u)
    # i64.lt_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqb t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_gt_s)
    # i64.gt_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqgt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_gt_u)
    # i64.gt_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqa t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_le_s)
    # i64.le_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqlteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_le_u)
    # i64.le_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqbeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ge_s)
    # i64.ge_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqgteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ge_u)
    # i64.ge_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x5b - 0x60: f32 comparison #
    ###############################

instructionLabel(_f32_eq)
    # f32.eq
    popFloat32FT1()
    popFloat32FT0()
    cfeq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_ne)
    # f32.ne
    popFloat32FT1()
    popFloat32FT0()
    cfnequn ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_lt)
    # f32.lt
    popFloat32FT1()
    popFloat32FT0()
    cflt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_gt)
    # f32.gt
    popFloat32FT1()
    popFloat32FT0()
    cfgt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_le)
    # f32.le
    popFloat32FT1()
    popFloat32FT0()
    cflteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_ge)
    # f32.ge
    popFloat32FT1()
    popFloat32FT0()
    cfgteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()


    ###############################
    # 0x61 - 0x66: f64 comparison #
    ###############################

instructionLabel(_f64_eq)
    # f64.eq
    popFloat64FT1()
    popFloat64FT0()
    cdeq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_ne)
    # f64.ne
    popFloat64FT1()
    popFloat64FT0()
    cdnequn ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_lt)
    # f64.lt
    popFloat64FT1()
    popFloat64FT0()
    cdlt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_gt)
    # f64.gt
    popFloat64FT1()
    popFloat64FT0()
    cdgt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_le)
    # f64.le
    popFloat64FT1()
    popFloat64FT0()
    cdlteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_ge)
    # f64.ge
    popFloat64FT1()
    popFloat64FT0()
    cdgteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x67 - 0x78: i32 operations #
    ###############################

instructionLabel(_i32_clz)
    # i32.clz
    popInt32(t0, t2)
    lzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ctz)
    # i32.ctz
    popInt32(t0, t2)
    tzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_popcnt)
    # i32.popcnt
    popInt32(t1, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcount) end)
    pushInt32(r1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_add)
    # i32.add
    popInt32(t1, t2)
    popInt32(t0, t2)
    addi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_sub)
    # i32.sub
    popInt32(t1, t2)
    popInt32(t0, t2)
    subi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_mul)
    # i32.mul
    popInt32(t1, t2)
    popInt32(t0, t2)
    muli t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_div_s)
    # i32.div_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    btiz t1, .ipint_i32_div_s_throwDivisionByZero

    bineq t1, -1, .ipint_i32_div_s_safe
    bieq t0, constexpr INT32_MIN, .ipint_i32_div_s_throwIntegerOverflow

.ipint_i32_div_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cdqi
        idivi t1
    elsif ARM64 or ARM64E or RISCV64
        divis t1, t0
    else
        error
    end
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_div_s_throwDivisionByZero:
    ipintException(DivisionByZero)

.ipint_i32_div_s_throwIntegerOverflow:
    ipintException(IntegerOverflow)

instructionLabel(_i32_div_u)
    # i32.div_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    btiz t1, .ipint_i32_div_u_throwDivisionByZero

    if X86_64
        xori t2, t2
        udivi t1
    elsif ARM64 or ARM64E or RISCV64
        divi t1, t0
    else
        error
    end
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_div_u_throwDivisionByZero:
    ipintException(DivisionByZero)

instructionLabel(_i32_rem_s)
    # i32.rem_s
    popInt32(t1, t2)
    popInt32(t0, t2)

    btiz t1, .ipint_i32_rem_s_throwDivisionByZero

    bineq t1, -1, .ipint_i32_rem_s_safe
    bineq t0, constexpr INT32_MIN, .ipint_i32_rem_s_safe

    move 0, t2
    jmp .ipint_i32_rem_s_return

.ipint_i32_rem_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cdqi
        idivi t1
    elsif ARM64 or ARM64E
        divis t1, t0, t2
        muli t1, t2
        subi t0, t2, t2
    elsif RISCV64
        remis t0, t1, t2
    else
        error
    end

.ipint_i32_rem_s_return:
    pushInt32(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_rem_s_throwDivisionByZero:
    ipintException(DivisionByZero)

instructionLabel(_i32_rem_u)
    # i32.rem_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    btiz t1, .ipint_i32_rem_u_throwDivisionByZero

    if X86_64
        xori t2, t2
        udivi t1
    elsif ARM64 or ARM64E
        divi t1, t0, t2
        muli t1, t2
        subi t0, t2, t2
    elsif RISCV64
        remi t0, t1, t2
    else
        error
    end
    pushInt32(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_rem_u_throwDivisionByZero:
    ipintException(DivisionByZero)

instructionLabel(_i32_and)
    # i32.and
    popInt32(t1, t2)
    popInt32(t0, t2)
    andi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_or)
    # i32.or
    popInt32(t1, t2)
    popInt32(t0, t2)
    ori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_xor)
    # i32.xor
    popInt32(t1, t2)
    popInt32(t0, t2)
    xori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shl)
    # i32.shl
    popInt32(t1, t2)
    popInt32(t0, t2)
    lshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shr_s)
    # i32.shr_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    rshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shr_u)
    # i32.shr_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    urshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_rotl)
    # i32.rotl
    popInt32(t1, t2)
    popInt32(t0, t2)
    lrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_rotr)
    # i32.rotr
    popInt32(t1, t2)
    popInt32(t0, t2)
    rrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x79 - 0x8a: i64 operations #
    ###############################

instructionLabel(_i64_clz)
    # i64.clz
    popInt64(t0, t2)
    lzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ctz)
    # i64.ctz
    popInt64(t0, t2)
    tzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_popcnt)
    # i64.popcnt
    popInt64(t1, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcountll) end)
    pushInt64(r1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_add)
    # i64.add
    popInt64(t1, t2)
    popInt64(t0, t2)
    addq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_sub)
    # i64.sub
    popInt64(t1, t2)
    popInt64(t0, t2)
    subq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_mul)
    # i64.mul
    popInt64(t1, t2)
    popInt64(t0, t2)
    mulq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_div_s)
    # i64.div_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    btqz t1, .ipint_i64_div_s_throwDivisionByZero

    bqneq t1, -1, .ipint_i64_div_s_safe
    bqeq t0, constexpr INT64_MIN, .ipint_i64_div_s_throwIntegerOverflow

.ipint_i64_div_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cqoq
        idivq t1
    elsif ARM64 or ARM64E or RISCV64
        divqs t1, t0
    else
        error
    end
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_div_s_throwDivisionByZero:
    ipintException(DivisionByZero)

.ipint_i64_div_s_throwIntegerOverflow:
    ipintException(IntegerOverflow)

instructionLabel(_i64_div_u)
    # i64.div_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    btqz t1, .ipint_i64_div_u_throwDivisionByZero

    if X86_64
        xorq t2, t2
        udivq t1
    elsif ARM64 or ARM64E or RISCV64
        divq t1, t0
    else
        error
    end
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_div_u_throwDivisionByZero:
    ipintException(DivisionByZero)

instructionLabel(_i64_rem_s)
    # i64.rem_s
    popInt64(t1, t2)
    popInt64(t0, t2)

    btqz t1, .ipint_i64_rem_s_throwDivisionByZero

    bqneq t1, -1, .ipint_i64_rem_s_safe
    bqneq t0, constexpr INT64_MIN, .ipint_i64_rem_s_safe

    move 0, t2
    jmp .ipint_i64_rem_s_return

.ipint_i64_rem_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cqoq
        idivq t1
    elsif ARM64 or ARM64E
        divqs t1, t0, t2
        mulq t1, t2
        subq t0, t2, t2
    elsif RISCV64
        remqs t0, t1, t2
    else
        error
    end

.ipint_i64_rem_s_return:
    pushInt64(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_rem_s_throwDivisionByZero:
    ipintException(DivisionByZero)

instructionLabel(_i64_rem_u)
    # i64.rem_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    btqz t1, .ipint_i64_rem_u_throwDivisionByZero

    if X86_64
        xorq t2, t2
        udivq t1
    elsif ARM64 or ARM64E
        divq t1, t0, t2
        mulq t1, t2
        subq t0, t2, t2
    elsif RISCV64
        remq t0, t1, t2
    else
        error
    end
    pushInt64(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_rem_u_throwDivisionByZero:
    ipintException(DivisionByZero)

instructionLabel(_i64_and)
    # i64.and
    popInt64(t1, t2)
    popInt64(t0, t2)
    andq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_or)
    # i64.or
    popInt64(t1, t2)
    popInt64(t0, t2)
    orq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_xor)
    # i64.xor
    popInt64(t1, t2)
    popInt64(t0, t2)
    xorq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shl)
    # i64.shl
    popInt64(t1, t2)
    popInt64(t0, t2)
    lshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shr_s)
    # i64.shr_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    rshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shr_u)
    # i64.shr_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    urshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_rotl)
    # i64.rotl
    popInt64(t1, t2)
    popInt64(t0, t2)
    lrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_rotr)
    # i64.rotr
    popInt64(t1, t2)
    popInt64(t0, t2)
    rrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x8b - 0x98: f32 operations #
    ###############################

instructionLabel(_f32_abs)
    # f32.abs
    popFloat32FT0()
    absf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_neg)
    # f32.neg
    popFloat32FT0()
    negf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_ceil)
    # f32.ceil
    popFloat32FT0()
    ceilf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_floor)
    # f32.floor
    popFloat32FT0()
    floorf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_trunc)
    # f32.trunc
    popFloat32FT0()
    truncatef ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_nearest)
    # f32.nearest
    popFloat32FT0()
    roundf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_sqrt)
    # f32.sqrt
    popFloat32FT0()
    sqrtf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()
    
instructionLabel(_f32_add)
    # f32.add
    popFloat32FT1()
    popFloat32FT0()
    addf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_sub)
    # f32.sub
    popFloat32FT1()
    popFloat32FT0()
    subf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_mul)
    # f32.mul
    popFloat32FT1()
    popFloat32FT0()
    mulf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_div)
    # f32.div
    popFloat32FT1()
    popFloat32FT0()
    divf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_min)
    # f32.min
    popFloat32FT1()
    popFloat32FT0()
    bfeq ft0, ft1, .ipint_f32_min_equal
    bflt ft0, ft1, .ipint_f32_min_lt
    bfgt ft0, ft1, .ipint_f32_min_return

.ipint_f32_min_NaN:
    addf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_equal:
    orf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_lt:
    moved ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_return:
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_max)
    # f32.max
    popFloat32FT1()
    popFloat32FT0()

    bfeq ft1, ft0, .ipint_f32_max_equal
    bflt ft1, ft0, .ipint_f32_max_lt
    bfgt ft1, ft0, .ipint_f32_max_return

.ipint_f32_max_NaN:
    addf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_equal:
    andf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_lt:
    moved ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_return:
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_copysign)
    # f32.copysign
    popFloat32FT1()
    popFloat32FT0()

    ff2i ft1, t1
    move 0x80000000, t2
    andi t2, t1

    ff2i ft0, t0
    move 0x7fffffff, t2
    andi t2, t0

    ori t1, t0
    fi2f t0, ft0

    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x99 - 0xa6: f64 operations #
    ###############################

instructionLabel(_f64_abs)
    # f64.abs
    popFloat64FT0()
    absd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_neg)
    # f64.neg
    popFloat64FT0()
    negd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_ceil)
    # f64.ceil
    popFloat64FT0()
    ceild ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_floor)
    # f64.floor
    popFloat64FT0()
    floord ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_trunc)
    # f64.trunc
    popFloat64FT0()
    truncated ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_nearest)
    # f64.nearest
    popFloat64FT0()
    roundd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_sqrt)
    # f64.sqrt
    popFloat64FT0()
    sqrtd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()
    
instructionLabel(_f64_add)
    # f64.add
    popFloat64FT1()
    popFloat64FT0()
    addd ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_sub)
    # f64.sub
    popFloat64FT1()
    popFloat64FT0()
    subd ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_mul)
    # f64.mul
    popFloat64FT1()
    popFloat64FT0()
    muld ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_div)
    # f64.div
    popFloat64FT1()
    popFloat64FT0()
    divd ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_min)
    # f64.min
    popFloat64FT1()
    popFloat64FT0()
    bdeq ft0, ft1, .ipint_f64_min_equal
    bdlt ft0, ft1, .ipint_f64_min_lt
    bdgt ft0, ft1, .ipint_f64_min_return

.ipint_f64_min_NaN:
    addd ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_equal:
    ord ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_lt:
    moved ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_return:
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_max)
    # f64.max
    popFloat64FT1()
    popFloat64FT0()

    bdeq ft1, ft0, .ipint_f64_max_equal
    bdlt ft1, ft0, .ipint_f64_max_lt
    bdgt ft1, ft0, .ipint_f64_max_return

.ipint_f64_max_NaN:
    addd ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_equal:
    andd ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_lt:
    moved ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_return:
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_copysign)
    # f64.copysign
    popFloat64FT1()
    popFloat64FT0()

    fd2q ft1, t1
    move 0x8000000000000000, t2
    andq t2, t1

    fd2q ft0, t0
    move 0x7fffffffffffffff, t2
    andq t2, t0

    orq t1, t0
    fq2d t0, ft0

    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

    ############################
    # 0xa7 - 0xc4: conversions #
    ############################

instructionLabel(_i32_wrap_i64)
    # because of how we store values on stack, do nothing
    advancePC(1)
    nextIPIntInstruction()


instructionLabel(_i32_trunc_f32_s)
    popFloat32FT0()
    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_trunc_i32_f32_s_outOfBoundsTrunc

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i32_f32_s_outOfBoundsTrunc

    truncatef2is ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f32_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i32_trunc_f32_u)
    popFloat32FT0()
    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_trunc_i32_f32_u_outOfBoundsTrunc

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i32_f32_u_outOfBoundsTrunc

    truncatef2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f32_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i32_trunc_f64_s)
    popFloat64FT0()
    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_trunc_i32_f64_s_outOfBoundsTrunc

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_trunc_i32_f64_s_outOfBoundsTrunc

    truncated2is ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f64_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i32_trunc_f64_u)
    popFloat64FT0()
    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_trunc_i32_f64_u_outOfBoundsTrunc

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_trunc_i32_f64_u_outOfBoundsTrunc

    truncated2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f64_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i64_extend_i32_s)
    popInt32(t0, t1)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend_i32_u)
    popInt32(t0, t1)
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_f32_s)
    popFloat32FT0()
    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_trunc_i64_f32_s_outOfBoundsTrunc

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i64_f32_s_outOfBoundsTrunc

    truncatef2qs ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i64_f32_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i64_trunc_f32_u)
    popFloat32FT0()
    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i64_f32_u_outOfBoundsTrunc

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_f32_u_outOfBoundsTrunc

    truncatef2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f32_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i64_trunc_f64_s)
    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .ipint_i64_f64_s_outOfBoundsTrunc

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_f64_s_outOfBoundsTrunc

    truncated2qs ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f64_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_i64_trunc_f64_u)
    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i64_f64_u_outOfBoundsTrunc

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_f64_u_outOfBoundsTrunc

    truncated2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f64_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)

instructionLabel(_f32_convert_i32_s)
    popInt32(t0, t1)
    ci2fs t0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_convert_i32_u)
    popInt32(t0, t1)
    ci2f t0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_convert_i64_s)
    popInt64(t0, t1)
    cq2fs t0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_convert_i64_u)
    popInt64(t0, t1)
    if X86_64
        cq2f t0, t1, ft0
    else
        cq2f t0, ft0
    end
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_demote_f64)
    popFloat64FT0()
    cd2f ft0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i32_s)
    popInt32(t0, t1)
    ci2ds t0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i32_u)
    popInt32(t0, t1)
    ci2d t0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i64_s)
    popInt64(t0, t1)
    cq2ds t0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i64_u)
    popInt64(t0, t1)
    if X86_64
        cq2d t0, t1, ft0
    else
        cq2d t0, ft0
    end
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_promote_f32)
    popFloat32FT0()
    cf2d ft0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_reinterpret_f32)
    popFloat32FT0()
    ff2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_reinterpret_f64)
    popFloat64FT0()
    fd2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_reinterpret_i32)
    pushInt32(t0)
    fi2f t0, ft0
    popFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_reinterpret_i64)
    pushInt64(t0)
    fq2d t0, ft0
    popFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_extend8_s)
    # i32.extend8_s
    popInt32(t0, t1)
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_extend16_s)
    # i32.extend8_s
    popInt32(t0, t1)
    sxh2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend8_s)
    # i64.extend8_s
    popInt64(t0, t1)
    sxb2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend16_s)
    # i64.extend8_s
    popInt64(t0, t1)
    sxh2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend32_s)
    # i64.extend8_s
    popInt64(t0, t1)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

reservedOpcode(0xc5)
reservedOpcode(0xc6)
reservedOpcode(0xc7)
reservedOpcode(0xc8)
reservedOpcode(0xc9)
reservedOpcode(0xca)
reservedOpcode(0xcb)
reservedOpcode(0xcc)
reservedOpcode(0xcd)
reservedOpcode(0xce)
reservedOpcode(0xcf)

    #####################
    # 0xd0 - 0xd2: refs #
    #####################

instructionLabel(_ref_null_t)
    loadi [PM, MC], t0
    pushQuad(t0)
    loadi 4[PM, MC], t0
    advancePC(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_ref_is_null)
    popQuad(t0, t1)
    cqeq t0, ValueNull, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_ref_func)
    move wasmInstance, a0
    loadi [PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_ref_func) end)
    pushQuad(t0)
    loadi 4[PM, MC], t0
    advancePC(t0)
    advanceMC(8)
    nextIPIntInstruction()

reservedOpcode(0xd3)
reservedOpcode(0xd4)
reservedOpcode(0xd5)
reservedOpcode(0xd6)
reservedOpcode(0xd7)
reservedOpcode(0xd8)
reservedOpcode(0xd9)
reservedOpcode(0xda)
reservedOpcode(0xdb)
reservedOpcode(0xdc)
reservedOpcode(0xdd)
reservedOpcode(0xde)
reservedOpcode(0xdf)
reservedOpcode(0xe0)
reservedOpcode(0xe1)
reservedOpcode(0xe2)
reservedOpcode(0xe3)
reservedOpcode(0xe4)
reservedOpcode(0xe5)
reservedOpcode(0xe6)
reservedOpcode(0xe7)
reservedOpcode(0xe8)
reservedOpcode(0xe9)
reservedOpcode(0xea)
reservedOpcode(0xeb)
reservedOpcode(0xec)
reservedOpcode(0xed)
reservedOpcode(0xee)
reservedOpcode(0xef)
reservedOpcode(0xf0)
reservedOpcode(0xf1)
reservedOpcode(0xf2)
reservedOpcode(0xf3)
reservedOpcode(0xf4)
reservedOpcode(0xf5)
reservedOpcode(0xf6)
reservedOpcode(0xf7)
reservedOpcode(0xf8)
reservedOpcode(0xf9)
reservedOpcode(0xfa)
reservedOpcode(0xfb)
instructionLabel(_fc_block)
    loadb 1[PB, PC], t0
    biaeq t0, 18, .ipint_fc_nonexistent
    # Security guarantee: always less than 18 (0x00 -> 0x11)
    if ARM64 or ARM64E
        pcrtoaddr _ipint_i32_trunc_sat_f32_s, t1
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
    elsif X86_64
        lshifti 4, t0
        leap (_ipint_i32_trunc_sat_f32_s), t1
        addq t1, t0
        emit "jmp *(%eax)"
    end

.ipint_fc_nonexistent:
    ipintException(Unreachable)

unimplementedInstruction(_simd)
reservedOpcode(0xfe)
reservedOpcode(0xff)

    #######################
    ## 0xFC instructions ##
    #######################

instructionLabel(_i32_trunc_sat_f32_s)
    popFloat32FT0()

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax

    truncatef2is ft0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i32_trunc_sat_f32_u)
    popFloat32FT0()

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax

    truncatef2i ft0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i32_trunc_sat_f64_s)
    popFloat64FT0()

    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax

    truncated2is ft0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i32_trunc_sat_f64_u)
    popFloat64FT0()

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2i ft0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f32_s)
    popFloat32FT0()

    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax

    truncatef2qs ft0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f32_u)
    popFloat32FT0()

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMin

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMax

    truncatef2q ft0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f64_s)
    popFloat64FT0()
    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncated2qs ft0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.outOfBoundsTruncSatMin:
    move (constexpr INT64_MIN), t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f64_u)
    popFloat64FT0()

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMin

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2q ft0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    pushInt64(t0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_memory_init)
    # memory.init
    popQuad(t0, t3) # n
    popQuad(t1, t3) # s
    popQuad(a2, t3) # d
    lshiftq 32, t1
    orq t1, t0
    move t0, a3
    loadi [PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_init) end)
    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_data_drop)
    # data.drop
    loadi [PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_data_drop) end)
    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_memory_copy)
    # memory.copy
    popQuad(a3, t0) # n
    popQuad(a2, t0) # s
    popQuad(a1, t0) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_copy) end)
    advancePC(4)
    nextIPIntInstruction()

instructionLabel(_memory_fill)
    # memory.fill
    popQuad(a3, t0) # n
    popQuad(a2, t0) # val
    popQuad(a1, t0) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_fill) end)
    advancePC(3)
    nextIPIntInstruction()

instructionLabel(_table_init)
    # memory.init
    popQuad(t0, t3) # n
    popQuad(t1, t3) # s
    popQuad(a2, t3) # d
    lshiftq 32, t1
    orq t1, t0
    move t0, a3
    leap [PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_init) end)
    loadq 8[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(16)
    nextIPIntInstruction()

instructionLabel(_elem_drop)
    # elem.drop
    loadi [PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_elem_drop) end)
    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_table_copy)
    # table.copy
    popQuad(t0, t3) # n
    popQuad(t1, t3) # s
    popQuad(a2, t3) # d
    lshiftq 32, t1
    orq t1, t0
    move t0, a3
    leap [PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_copy) end)
    loadq 8[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(16)
    nextIPIntInstruction()

instructionLabel(_table_grow)
    # table.grow
    loadi [PM, MC], a1
    popQuad(a3, t0) # n
    popQuad(a2, t0) # fill
    operationCall(macro() cCall4(_ipint_extern_table_grow) end)
    pushQuad(t0)
    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_table_size)
    # table.size
    loadi [PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_table_size) end)
    pushQuad(t0)
    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_table_fill)
    # table.fill
    popQuad(t0, t3) # n
    popQuad(a2, t3) # val
    popQuad(t3, t1) # i
    lshiftq 32, t3
    orq t0, t3
    loadi [PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_fill) end)
    loadi 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

    ##################################
    ## "Out of line" logic for call ##
    ##################################

_ipint_call_impl:
    # 0 - 3: function index
    # 4 - 7: PC post call
    # 8 - 23: offsets for GPR
    # 24 - 39: offsets for FPR
    # 40 - 42: length of stack data
    # 42 - 44: number of arguments
    # 44 - 46: number of stack arguments
    # 46 - x: offsets

    # function index
    loadi [PM, MC], t0

    # Get function data
    move t0, a1
    move wasmInstance, a0
    cCall2(_ipint_extern_call)

.ipint_call_common:

    move r0, ws0
    move r1, wasmInstance

    # Set up stack first
    loadh 44[PM, MC], t2
    move t2, t3
    # 8B per argument on the stack
    lshiftq 3, t2
    # round up to nearest multiple of 16
    # keep an extra 16 for saved PL
    addq 24, t2
    andq -16, t2
    move sp, t1
    subp t2, t1
    leap 46[PM, MC], t2
    move 0, t0
.ipint_call_stack_loop:
    loadh [t2], t4
    lshiftq 4, t4
    bqeq t0, t3, .ipint_call_stack_end
    loadq [sp, t4], t4
    addp 2, t2
    storeq t4, [t1]
    addp 8, t1
    addq 1, t0
    jmp .ipint_call_stack_loop
.ipint_call_stack_end:
    move MC, PB
    pushQuad(PL)

    # Set up arguments in registers
    loadh 8[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a0
    loadh 10[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a1
    loadh 12[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a2
    loadh 14[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a3
    loadh 16[PM, PB], ws1
if ARM64 or ARM64E
    lshiftq 4, ws1
    loadq 16[sp, ws1], a4
    loadh 18[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a5
    loadh 20[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a6
    loadh 22[PM, PB], ws1
    lshiftq 4, ws1
    loadq 16[sp, ws1], a7
end

    loadh 24[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa0
    loadh 26[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa1
    loadh 28[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa2
    loadh 30[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa3
    loadh 32[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa4
    loadh 34[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa5
if ARM64 or ARM64E
    loadh 36[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa6
    loadh 38[PM, PB], ws1
    lshiftq 4, ws1
    loadd 16[sp, ws1], wfa7
end

    loadq [sp], ws1
    addq 16, sp

    # reset SP to top of params
    loadh 44[PM, PB], ws1
    # 8B per argument on the stack
    lshiftq 3, ws1
    # round up to nearest multiple of 16
    # keep an extra 16 for saved PL
    addq 24, ws1
    andq -16, ws1
    subp ws1, sp

    # Space for caller frame
    subp FirstArgumentOffset - 16, sp

    # Make the call
    move wasmInstance, PM
    call ws0, JSEntrySlowPathPtrTag

    addq FirstArgumentOffset - 16, sp

    # Post-call: restore instance and MC

    move PM, wasmInstance
    getIPIntCallee()
    # Saved MC
    move PB, ws1
    # Throw away PM value since it's already in wasmInstance
    popQuad(PB, PM)
    loadp Wasm::IPIntCallee::m_metadata[ws0], PM

    # ws0 = limit
    # csr3 = index
    # csr4 = value
    move ws1, csr3
    addq 40, csr3
    # Load size of parameter group
    loadh [PM, csr3], ws0
    loadh 2[PM, csr3], csr4
    addq ws0, csr3

    # Pop all arguments off stack
    lshiftq 4, csr4
    addp csr4, sp

    # Load size of return group
    loadh [PM, csr3], ws0
    addq csr3, ws0
.ipint_call_ret_loop:
    bqeq ws0, csr3, .ipint_call_ret_loop_end
    loadb 2[PM, csr3], csr4
    bqeq csr4, 0, .ipint_call_ret_loop_end
    addq 1, csr3
    bqgteq csr4, 6, .ipint_call_ret_stack
    bqeq csr4, 5, .ipint_call_ret_fpr
    pushQuad(t0)
    jmp .ipint_call_ret_loop
.ipint_call_ret_fpr:
    pushFPR()
    jmp .ipint_call_ret_loop
.ipint_call_ret_stack:
    # TODO
    break
.ipint_call_ret_loop_end:

    move ws0, MC
    move PB, PL

    # Restore PC
    loadi 4[PM, ws1], PC
    # Restore ws1
    getIPIntCallee()
    loadp Wasm::IPIntCallee::m_bytecode[ws0], PB

    # Restore IB
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end

    ipintReloadMemory()

    nextIPIntInstruction()

# Put all operations before this `else`, or else 32-bit architectures will fail to build.
else
# For 32-bit architectures: make sure that the assertions can still find the labels
unimplementedInstruction(_unreachable)
unimplementedInstruction(_nop)
unimplementedInstruction(_block)
unimplementedInstruction(_loop)
unimplementedInstruction(_if)
unimplementedInstruction(_else)
reservedOpcode(0x06)
reservedOpcode(0x07)
reservedOpcode(0x08)
reservedOpcode(0x09)
reservedOpcode(0x0a)
unimplementedInstruction(_end)
unimplementedInstruction(_br)
unimplementedInstruction(_br_if)
unimplementedInstruction(_br_table)
unimplementedInstruction(_return)
unimplementedInstruction(_call)
unimplementedInstruction(_call_indirect)
reservedOpcode(0x12)
reservedOpcode(0x13)
reservedOpcode(0x14)
reservedOpcode(0x15)
reservedOpcode(0x16)
reservedOpcode(0x17)
reservedOpcode(0x18)
reservedOpcode(0x19)
unimplementedInstruction(_drop)
unimplementedInstruction(_select)
unimplementedInstruction(_select_t)
reservedOpcode(0x1d)
reservedOpcode(0x1e)
reservedOpcode(0x1f)
unimplementedInstruction(_local_get)
unimplementedInstruction(_local_set)
unimplementedInstruction(_local_tee)
unimplementedInstruction(_global_get)
unimplementedInstruction(_global_set)
unimplementedInstruction(_table_get)
unimplementedInstruction(_table_set)
reservedOpcode(0x27)
unimplementedInstruction(_i32_load_mem)
unimplementedInstruction(_i64_load_mem)
unimplementedInstruction(_f32_load_mem)
unimplementedInstruction(_f64_load_mem)
unimplementedInstruction(_i32_load8s_mem)
unimplementedInstruction(_i32_load8u_mem)
unimplementedInstruction(_i32_load16s_mem)
unimplementedInstruction(_i32_load16u_mem)
unimplementedInstruction(_i64_load8s_mem)
unimplementedInstruction(_i64_load8u_mem)
unimplementedInstruction(_i64_load16s_mem)
unimplementedInstruction(_i64_load16u_mem)
unimplementedInstruction(_i64_load32s_mem)
unimplementedInstruction(_i64_load32u_mem)
unimplementedInstruction(_i32_store_mem)
unimplementedInstruction(_i64_store_mem)
unimplementedInstruction(_f32_store_mem)
unimplementedInstruction(_f64_store_mem)
unimplementedInstruction(_i32_store8_mem)
unimplementedInstruction(_i32_store16_mem)
unimplementedInstruction(_i64_store8_mem)
unimplementedInstruction(_i64_store16_mem)
unimplementedInstruction(_i64_store32_mem)
unimplementedInstruction(_memory_size)
unimplementedInstruction(_memory_grow)
unimplementedInstruction(_i32_const)
unimplementedInstruction(_i64_const)
unimplementedInstruction(_f32_const)
unimplementedInstruction(_f64_const)
unimplementedInstruction(_i32_eqz)
unimplementedInstruction(_i32_eq)
unimplementedInstruction(_i32_ne)
unimplementedInstruction(_i32_lt_s)
unimplementedInstruction(_i32_lt_u)
unimplementedInstruction(_i32_gt_s)
unimplementedInstruction(_i32_gt_u)
unimplementedInstruction(_i32_le_s)
unimplementedInstruction(_i32_le_u)
unimplementedInstruction(_i32_ge_s)
unimplementedInstruction(_i32_ge_u)
unimplementedInstruction(_i64_eqz)
unimplementedInstruction(_i64_eq)
unimplementedInstruction(_i64_ne)
unimplementedInstruction(_i64_lt_s)
unimplementedInstruction(_i64_lt_u)
unimplementedInstruction(_i64_gt_s)
unimplementedInstruction(_i64_gt_u)
unimplementedInstruction(_i64_le_s)
unimplementedInstruction(_i64_le_u)
unimplementedInstruction(_i64_ge_s)
unimplementedInstruction(_i64_ge_u)
unimplementedInstruction(_f32_eq)
unimplementedInstruction(_f32_ne)
unimplementedInstruction(_f32_lt)
unimplementedInstruction(_f32_gt)
unimplementedInstruction(_f32_le)
unimplementedInstruction(_f32_ge)
unimplementedInstruction(_f64_eq)
unimplementedInstruction(_f64_ne)
unimplementedInstruction(_f64_lt)
unimplementedInstruction(_f64_gt)
unimplementedInstruction(_f64_le)
unimplementedInstruction(_f64_ge)
unimplementedInstruction(_i32_clz)
unimplementedInstruction(_i32_ctz)
unimplementedInstruction(_i32_popcnt)
unimplementedInstruction(_i32_add)
unimplementedInstruction(_i32_sub)
unimplementedInstruction(_i32_mul)
unimplementedInstruction(_i32_div_s)
unimplementedInstruction(_i32_div_u)
unimplementedInstruction(_i32_rem_s)
unimplementedInstruction(_i32_rem_u)
unimplementedInstruction(_i32_and)
unimplementedInstruction(_i32_or)
unimplementedInstruction(_i32_xor)
unimplementedInstruction(_i32_shl)
unimplementedInstruction(_i32_shr_s)
unimplementedInstruction(_i32_shr_u)
unimplementedInstruction(_i32_rotl)
unimplementedInstruction(_i32_rotr)
unimplementedInstruction(_i64_clz)
unimplementedInstruction(_i64_ctz)
unimplementedInstruction(_i64_popcnt)
unimplementedInstruction(_i64_add)
unimplementedInstruction(_i64_sub)
unimplementedInstruction(_i64_mul)
unimplementedInstruction(_i64_div_s)
unimplementedInstruction(_i64_div_u)
unimplementedInstruction(_i64_rem_s)
unimplementedInstruction(_i64_rem_u)
unimplementedInstruction(_i64_and)
unimplementedInstruction(_i64_or)
unimplementedInstruction(_i64_xor)
unimplementedInstruction(_i64_shl)
unimplementedInstruction(_i64_shr_s)
unimplementedInstruction(_i64_shr_u)
unimplementedInstruction(_i64_rotl)
unimplementedInstruction(_i64_rotr)
unimplementedInstruction(_f32_abs)
unimplementedInstruction(_f32_neg)
unimplementedInstruction(_f32_ceil)
unimplementedInstruction(_f32_floor)
unimplementedInstruction(_f32_trunc)
unimplementedInstruction(_f32_nearest)
unimplementedInstruction(_f32_sqrt)
unimplementedInstruction(_f32_add)
unimplementedInstruction(_f32_sub)
unimplementedInstruction(_f32_mul)
unimplementedInstruction(_f32_div)
unimplementedInstruction(_f32_min)
unimplementedInstruction(_f32_max)
unimplementedInstruction(_f32_copysign)
unimplementedInstruction(_f64_abs)
unimplementedInstruction(_f64_neg)
unimplementedInstruction(_f64_ceil)
unimplementedInstruction(_f64_floor)
unimplementedInstruction(_f64_trunc)
unimplementedInstruction(_f64_nearest)
unimplementedInstruction(_f64_sqrt)
unimplementedInstruction(_f64_add)
unimplementedInstruction(_f64_sub)
unimplementedInstruction(_f64_mul)
unimplementedInstruction(_f64_div)
unimplementedInstruction(_f64_min)
unimplementedInstruction(_f64_max)
unimplementedInstruction(_f64_copysign)
unimplementedInstruction(_i32_wrap_i64)
unimplementedInstruction(_i32_trunc_f32_s)
unimplementedInstruction(_i32_trunc_f32_u)
unimplementedInstruction(_i32_trunc_f64_s)
unimplementedInstruction(_i32_trunc_f64_u)
unimplementedInstruction(_i64_extend_i32_s)
unimplementedInstruction(_i64_extend_i32_u)
unimplementedInstruction(_i64_trunc_f32_s)
unimplementedInstruction(_i64_trunc_f32_u)
unimplementedInstruction(_i64_trunc_f64_s)
unimplementedInstruction(_i64_trunc_f64_u)
unimplementedInstruction(_f32_convert_i32_s)
unimplementedInstruction(_f32_convert_i32_u)
unimplementedInstruction(_f32_convert_i64_s)
unimplementedInstruction(_f32_convert_i64_u)
unimplementedInstruction(_f32_demote_f64)
unimplementedInstruction(_f64_convert_i32_s)
unimplementedInstruction(_f64_convert_i32_u)
unimplementedInstruction(_f64_convert_i64_s)
unimplementedInstruction(_f64_convert_i64_u)
unimplementedInstruction(_f64_promote_f32)
unimplementedInstruction(_i32_reinterpret_f32)
unimplementedInstruction(_i64_reinterpret_f64)
unimplementedInstruction(_f32_reinterpret_i32)
unimplementedInstruction(_f64_reinterpret_i64)
unimplementedInstruction(_i32_extend8_s)
unimplementedInstruction(_i32_extend16_s)
unimplementedInstruction(_i64_extend8_s)
unimplementedInstruction(_i64_extend16_s)
unimplementedInstruction(_i64_extend32_s)
reservedOpcode(0xc5)
reservedOpcode(0xc6)
reservedOpcode(0xc7)
reservedOpcode(0xc8)
reservedOpcode(0xc9)
reservedOpcode(0xca)
reservedOpcode(0xcb)
reservedOpcode(0xcc)
reservedOpcode(0xcd)
reservedOpcode(0xce)
reservedOpcode(0xcf)
unimplementedInstruction(_ref_null_t)
unimplementedInstruction(_ref_is_null)
unimplementedInstruction(_ref_func)
reservedOpcode(0xd3)
reservedOpcode(0xd4)
reservedOpcode(0xd5)
reservedOpcode(0xd6)
reservedOpcode(0xd7)
reservedOpcode(0xd8)
reservedOpcode(0xd9)
reservedOpcode(0xda)
reservedOpcode(0xdb)
reservedOpcode(0xdc)
reservedOpcode(0xdd)
reservedOpcode(0xde)
reservedOpcode(0xdf)
reservedOpcode(0xe0)
reservedOpcode(0xe1)
reservedOpcode(0xe2)
reservedOpcode(0xe3)
reservedOpcode(0xe4)
reservedOpcode(0xe5)
reservedOpcode(0xe6)
reservedOpcode(0xe7)
reservedOpcode(0xe8)
reservedOpcode(0xe9)
reservedOpcode(0xea)
reservedOpcode(0xeb)
reservedOpcode(0xec)
reservedOpcode(0xed)
reservedOpcode(0xee)
reservedOpcode(0xef)
reservedOpcode(0xf0)
reservedOpcode(0xf1)
reservedOpcode(0xf2)
reservedOpcode(0xf3)
reservedOpcode(0xf4)
reservedOpcode(0xf5)
reservedOpcode(0xf6)
reservedOpcode(0xf7)
reservedOpcode(0xf8)
reservedOpcode(0xf9)
reservedOpcode(0xfa)
reservedOpcode(0xfb)
unimplementedInstruction(_fc_block)
unimplementedInstruction(_simd)
reservedOpcode(0xfe)
reservedOpcode(0xff)
end
