
# IPInt debugger scripts for looking at the state of the program
# Run with:
# lldb -s debug_ipint.lldb -- jsc (args)

import lldb

import bisect
import platform
import struct
from pathlib import Path

# ANSI escapes
RESET = '\033[0m'
BOLD = '\033[1m'
DIM = '\033[2m'
RED = '\033[31m'
CYAN = '\033[36m'

machine = platform.machine().lower()

if machine in ['arm64', 'aarch64']:
    PC_REG = 'x26'
    MC_REG = 'x25'
    PL_REG = 'x6'
elif machine in ['x86_64', 'amd64']:
    PC_REG = 'r13'
    MC_REG = 'r12'
    PL_REG = 'r10'
else:
    raise RuntimeError(f"Unsupported architecture: {machine}")

# Instruction opcode symbols from InPlaceInterpreter64.asm, without the "ipint_" prefix.
# The order should match the address space order.
IPINT_INSTRUCTIONS = [
    # 0x00
    'unreachable', 'nop', 'block', 'loop', 'if', 'else', 'try', 'catch',
    'throw', 'rethrow', 'throw_ref', 'end', 'br', 'br_if', 'br_table', 'return',
    # 0x10
    'call', 'call_indirect', 'return_call', 'return_call_indirect', 'call_ref', 'return_call_ref',
    'delegate', 'catch_all', 'drop', 'select', 'select_t', 'try_table',
    # 0x20
    'local_get', 'local_set', 'local_tee', 'global_get', 'global_set', 'table_get', 'table_set',
    'i32_load_mem', 'i64_load_mem', 'f32_load_mem', 'f64_load_mem', 'i32_load8s_mem', 'i32_load8u_mem', 'i32_load16s_mem', 'i32_load16u_mem',
    # 0x30
    'i64_load8s_mem', 'i64_load8u_mem', 'i64_load16s_mem', 'i64_load16u_mem', 'i64_load32s_mem', 'i64_load32u_mem', 'i32_store_mem', 'i64_store_mem',
    'f32_store_mem', 'f64_store_mem', 'i32_store8_mem', 'i32_store16_mem', 'i64_store8_mem', 'i64_store16_mem', 'i64_store32_mem', 'memory_size',
    # 0x40
    'memory_grow', 'i32_const', 'i64_const', 'f32_const', 'f64_const', 'i32_eqz', 'i32_eq', 'i32_ne',
    'i32_lt_s', 'i32_lt_u', 'i32_gt_s', 'i32_gt_u', 'i32_le_s', 'i32_le_u', 'i32_ge_s', 'i32_ge_u',
    # 0x50
    'i64_eqz', 'i64_eq', 'i64_ne', 'i64_lt_s', 'i64_lt_u', 'i64_gt_s', 'i64_gt_u', 'i64_le_s',
    'i64_le_u', 'i64_ge_s', 'i64_ge_u', 'f32_eq', 'f32_ne', 'f32_lt', 'f32_gt', 'f32_le',
    # 0x60
    'f32_ge', 'f64_eq', 'f64_ne', 'f64_lt', 'f64_gt', 'f64_le', 'f64_ge', 'i32_clz',
    'i32_ctz', 'i32_popcnt', 'i32_add', 'i32_sub', 'i32_mul', 'i32_div_s', 'i32_div_u', 'i32_rem_s',
    # 0x70
    'i32_rem_u', 'i32_and', 'i32_or', 'i32_xor', 'i32_shl', 'i32_shr_s', 'i32_shr_u', 'i32_rotl',
    'i32_rotr', 'i64_clz', 'i64_ctz', 'i64_popcnt', 'i64_add', 'i64_sub', 'i64_mul', 'i64_div_s',
    # 0x80
    'i64_div_u', 'i64_rem_s', 'i64_rem_u', 'i64_and', 'i64_or', 'i64_xor', 'i64_shl', 'i64_shr_s',
    'i64_shr_u', 'i64_rotl', 'i64_rotr', 'f32_abs', 'f32_neg', 'f32_ceil', 'f32_floor', 'f32_trunc',
    # 0x90
    'f32_nearest', 'f32_sqrt', 'f32_add', 'f32_sub', 'f32_mul', 'f32_div', 'f32_min', 'f32_max',
    'f32_copysign', 'f64_abs', 'f64_neg', 'f64_ceil', 'f64_floor', 'f64_trunc', 'f64_nearest', 'f64_sqrt',
    # 0xA0
    'f64_add', 'f64_sub', 'f64_mul', 'f64_div', 'f64_min', 'f64_max', 'f64_copysign', 'i32_wrap_i64',
    'i32_trunc_f32_s', 'i32_trunc_f32_u', 'i32_trunc_f64_s', 'i32_trunc_f64_u', 'i64_extend_i32_s', 'i64_extend_i32_u', 'i64_trunc_f32_s', 'i64_trunc_f32_u',
    # 0xB0
    'i64_trunc_f64_s', 'i64_trunc_f64_u', 'f32_convert_i32_s', 'f32_convert_i32_u', 'f32_convert_i64_s', 'f32_convert_i64_u', 'f32_demote_f64', 'f64_convert_i32_s',
    'f64_convert_i32_u', 'f64_convert_i64_s', 'f64_convert_i64_u', 'f64_promote_f32', 'i32_reinterpret_f32', 'i64_reinterpret_f64', 'f32_reinterpret_i32', 'f64_reinterpret_i64',
    # 0xC0
    'i32_extend8_s', 'i32_extend16_s', 'i64_extend8_s', 'i64_extend16_s', 'i64_extend32_s',
    # 0xD0
    'ref_null_t', 'ref_is_null', 'ref_func', 'ref_eq', 'ref_as_non_null', 'br_on_null', 'br_on_non_null',
    # 0xFB
    'gc_prefix', 'conversion_prefix', 'simd_prefix', 'atomic_prefix',

    # extended
    'struct_new', 'struct_new_default', 'struct_get',
    'struct_get_s', 'struct_get_u', 'struct_set', 'array_new', 'array_new_default',
    'array_new_fixed', 'array_new_data', 'array_new_elem', 'array_get', 'array_get_s',
    'array_get_u', 'array_set', 'array_len', 'array_fill', 'array_copy',
    'array_init_data', 'array_init_elem', 'ref_test', 'ref_test_nullable', 'ref_cast',
    'ref_cast_nullable', 'br_on_cast', 'br_on_cast_fail', 'any_convert_extern',
    'extern_convert_any', 'ref_i31', 'i31_get_s', 'i31_get_u', 'i32_trunc_sat_f32_s',
    'i32_trunc_sat_f32_u', 'i32_trunc_sat_f64_s', 'i32_trunc_sat_f64_u', 'i64_trunc_sat_f32_s',
    'i64_trunc_sat_f32_u', 'i64_trunc_sat_f64_s', 'i64_trunc_sat_f64_u', 'memory_init',
    'data_drop', 'memory_copy', 'memory_fill', 'table_init', 'elem_drop', 'table_copy',
    'table_grow', 'table_size', 'table_fill',

    # extended SIMD instructions (0xFD prefix)
    'simd_v128_load_mem', 'simd_v128_load_8x8s_mem', 'simd_v128_load_8x8u_mem', 'simd_v128_load_16x4s_mem',
    'simd_v128_load_16x4u_mem', 'simd_v128_load_32x2s_mem', 'simd_v128_load_32x2u_mem', 'simd_v128_load8_splat_mem',
    'simd_v128_load16_splat_mem', 'simd_v128_load32_splat_mem', 'simd_v128_load64_splat_mem', 'simd_v128_store_mem',
    'simd_v128_const', 'simd_i8x16_shuffle', 'simd_i8x16_swizzle', 'simd_i8x16_splat',
    'simd_i16x8_splat', 'simd_i32x4_splat', 'simd_i64x2_splat', 'simd_f32x4_splat',
    'simd_f64x2_splat', 'simd_i8x16_extract_lane_s', 'simd_i8x16_extract_lane_u', 'simd_i8x16_replace_lane',
    'simd_i16x8_extract_lane_s', 'simd_i16x8_extract_lane_u', 'simd_i16x8_replace_lane', 'simd_i32x4_extract_lane',
    'simd_i32x4_replace_lane', 'simd_i64x2_extract_lane', 'simd_i64x2_replace_lane', 'simd_f32x4_extract_lane',
    'simd_f32x4_replace_lane', 'simd_f64x2_extract_lane', 'simd_f64x2_replace_lane', 'simd_i8x16_eq',
    'simd_i8x16_ne', 'simd_i8x16_lt_s', 'simd_i8x16_lt_u', 'simd_i8x16_gt_s',
    'simd_i8x16_gt_u', 'simd_i8x16_le_s', 'simd_i8x16_le_u', 'simd_i8x16_ge_s',
    'simd_i8x16_ge_u', 'simd_i16x8_eq', 'simd_i16x8_ne', 'simd_i16x8_lt_s',
    'simd_i16x8_lt_u', 'simd_i16x8_gt_s', 'simd_i16x8_gt_u', 'simd_i16x8_le_s',
    'simd_i16x8_le_u', 'simd_i16x8_ge_s', 'simd_i16x8_ge_u', 'simd_i32x4_eq',
    'simd_i32x4_ne', 'simd_i32x4_lt_s', 'simd_i32x4_lt_u', 'simd_i32x4_gt_s',
    'simd_i32x4_gt_u', 'simd_i32x4_le_s', 'simd_i32x4_le_u', 'simd_i32x4_ge_s',
    'simd_i32x4_ge_u', 'simd_f32x4_eq', 'simd_f32x4_ne', 'simd_f32x4_lt',
    'simd_f32x4_gt', 'simd_f32x4_le', 'simd_f32x4_ge', 'simd_f64x2_eq',
    'simd_f64x2_ne', 'simd_f64x2_lt', 'simd_f64x2_gt', 'simd_f64x2_le',
    'simd_f64x2_ge', 'simd_v128_not', 'simd_v128_and', 'simd_v128_andnot',
    'simd_v128_or', 'simd_v128_xor', 'simd_v128_bitselect', 'simd_v128_any_true',
    'simd_v128_load8_lane_mem', 'simd_v128_load16_lane_mem', 'simd_v128_load32_lane_mem', 'simd_v128_load64_lane_mem',
    'simd_v128_store8_lane_mem', 'simd_v128_store16_lane_mem', 'simd_v128_store32_lane_mem', 'simd_v128_store64_lane_mem',
    'simd_v128_load32_zero_mem', 'simd_v128_load64_zero_mem', 'simd_f32x4_demote_f64x2_zero', 'simd_f64x2_promote_low_f32x4',
    'simd_i8x16_abs', 'simd_i8x16_neg', 'simd_i8x16_popcnt', 'simd_i8x16_all_true',
    'simd_i8x16_bitmask', 'simd_i8x16_narrow_i16x8_s', 'simd_i8x16_narrow_i16x8_u', 'simd_f32x4_ceil',
    'simd_f32x4_floor', 'simd_f32x4_trunc', 'simd_f32x4_nearest', 'simd_i8x16_shl',
    'simd_i8x16_shr_s', 'simd_i8x16_shr_u', 'simd_i8x16_add', 'simd_i8x16_add_sat_s',
    'simd_i8x16_add_sat_u', 'simd_i8x16_sub', 'simd_i8x16_sub_sat_s', 'simd_i8x16_sub_sat_u',
    'simd_f64x2_ceil', 'simd_f64x2_floor', 'simd_i8x16_min_s', 'simd_i8x16_min_u',
    'simd_i8x16_max_s', 'simd_i8x16_max_u', 'simd_f64x2_trunc', 'simd_i8x16_avgr_u',
    'simd_i16x8_extadd_pairwise_i8x16_s', 'simd_i16x8_extadd_pairwise_i8x16_u', 'simd_i32x4_extadd_pairwise_i16x8_s', 'simd_i32x4_extadd_pairwise_i16x8_u',
    'simd_i16x8_abs', 'simd_i16x8_neg', 'simd_i16x8_q15mulr_sat_s', 'simd_i16x8_all_true',
    'simd_i16x8_bitmask', 'simd_i16x8_narrow_i32x4_s', 'simd_i16x8_narrow_i32x4_u', 'simd_i16x8_extend_low_i8x16_s',
    'simd_i16x8_extend_high_i8x16_s', 'simd_i16x8_extend_low_i8x16_u', 'simd_i16x8_extend_high_i8x16_u', 'simd_i16x8_shl',
    'simd_i16x8_shr_s', 'simd_i16x8_shr_u', 'simd_i16x8_add', 'simd_i16x8_add_sat_s',
    'simd_i16x8_add_sat_u', 'simd_i16x8_sub', 'simd_i16x8_sub_sat_s', 'simd_i16x8_sub_sat_u',
    'simd_f64x2_nearest', 'simd_i16x8_mul', 'simd_i16x8_min_s', 'simd_i16x8_min_u',
    'simd_i16x8_max_s', 'simd_i16x8_max_u', 'simd_i16x8_avgr_u', 'simd_i16x8_extmul_low_i8x16_s',
    'simd_i16x8_extmul_high_i8x16_s', 'simd_i16x8_extmul_low_i8x16_u', 'simd_i16x8_extmul_high_i8x16_u', 'simd_i32x4_abs',
    'simd_i32x4_neg', 'simd_i32x4_all_true', 'simd_i32x4_bitmask', 'simd_i32x4_extend_low_i16x8_s',
    'simd_i32x4_extend_high_i16x8_s', 'simd_i32x4_extend_low_i16x8_u', 'simd_i32x4_extend_high_i16x8_u', 'simd_i32x4_shl',
    'simd_i32x4_shr_s', 'simd_i32x4_shr_u', 'simd_i32x4_add', 'simd_i32x4_sub',
    'simd_i32x4_mul', 'simd_i32x4_min_s', 'simd_i32x4_min_u', 'simd_i32x4_max_s',
    'simd_i32x4_max_u', 'simd_i32x4_dot_i16x8_s', 'simd_i32x4_extmul_low_i16x8_s', 'simd_i32x4_extmul_high_i16x8_s',
    'simd_i32x4_extmul_low_i16x8_u', 'simd_i32x4_extmul_high_i16x8_u', 'simd_i64x2_abs', 'simd_i64x2_neg',
    'simd_i64x2_all_true', 'simd_i64x2_bitmask', 'simd_i64x2_extend_low_i32x4_s', 'simd_i64x2_extend_high_i32x4_s',
    'simd_i64x2_extend_low_i32x4_u', 'simd_i64x2_extend_high_i32x4_u', 'simd_i64x2_shl', 'simd_i64x2_shr_s',
    'simd_i64x2_shr_u', 'simd_i64x2_add', 'simd_i64x2_sub', 'simd_i64x2_mul',
    'simd_i64x2_eq', 'simd_i64x2_ne', 'simd_i64x2_lt_s', 'simd_i64x2_gt_s',
    'simd_i64x2_le_s', 'simd_i64x2_ge_s', 'simd_i64x2_extmul_low_i32x4_s', 'simd_i64x2_extmul_high_i32x4_s',
    'simd_i64x2_extmul_low_i32x4_u', 'simd_i64x2_extmul_high_i32x4_u', 'simd_f32x4_abs', 'simd_f32x4_neg',
    'simd_f32x4_sqrt', 'simd_f32x4_add', 'simd_f32x4_sub', 'simd_f32x4_mul',
    'simd_f32x4_div', 'simd_f32x4_min', 'simd_f32x4_max', 'simd_f32x4_pmin',
    'simd_f32x4_pmax', 'simd_f64x2_abs', 'simd_f64x2_neg', 'simd_f64x2_sqrt',
    'simd_f64x2_add', 'simd_f64x2_sub', 'simd_f64x2_mul', 'simd_f64x2_div',
    'simd_f64x2_min', 'simd_f64x2_max', 'simd_f64x2_pmin', 'simd_f64x2_pmax',
    'simd_i32x4_trunc_sat_f32x4_s', 'simd_i32x4_trunc_sat_f32x4_u', 'simd_f32x4_convert_i32x4_s', 'simd_f32x4_convert_i32x4_u',
    'simd_i32x4_trunc_sat_f64x2_s_zero', 'simd_i32x4_trunc_sat_f64x2_u_zero', 'simd_f64x2_convert_low_i32x4_s', 'simd_f64x2_convert_low_i32x4_u',

    # extended atomic instructions
    'memory_atomic_notify', 'memory_atomic_wait32', 'memory_atomic_wait64', 'atomic_fence',
    'i32_atomic_load', 'i64_atomic_load', 'i32_atomic_load8_u', 'i32_atomic_load16_u',
    'i64_atomic_load8_u', 'i64_atomic_load16_u', 'i64_atomic_load32_u', 'i32_atomic_store',
    'i64_atomic_store', 'i32_atomic_store8_u', 'i32_atomic_store16_u', 'i64_atomic_store8_u',
    'i64_atomic_store16_u', 'i64_atomic_store32_u', 'i32_atomic_rmw_add', 'i64_atomic_rmw_add',
    'i32_atomic_rmw8_add_u', 'i32_atomic_rmw16_add_u', 'i64_atomic_rmw8_add_u',
    'i64_atomic_rmw16_add_u', 'i64_atomic_rmw32_add_u', 'i32_atomic_rmw_sub', 'i64_atomic_rmw_sub',
    'i32_atomic_rmw8_sub_u', 'i32_atomic_rmw16_sub_u', 'i64_atomic_rmw8_sub_u',
    'i64_atomic_rmw16_sub_u', 'i64_atomic_rmw32_sub_u', 'i32_atomic_rmw_and',
    'i64_atomic_rmw_and', 'i32_atomic_rmw8_and_u', 'i32_atomic_rmw16_and_u',
    'i64_atomic_rmw8_and_u', 'i64_atomic_rmw16_and_u', 'i64_atomic_rmw32_and_u',
    'i32_atomic_rmw_or', 'i64_atomic_rmw_or', 'i32_atomic_rmw8_or_u', 'i32_atomic_rmw16_or_u',
    'i64_atomic_rmw8_or_u', 'i64_atomic_rmw16_or_u', 'i64_atomic_rmw32_or_u', 'i32_atomic_rmw_xor',
    'i64_atomic_rmw_xor', 'i32_atomic_rmw8_xor_u', 'i32_atomic_rmw16_xor_u', 'i64_atomic_rmw8_xor_u',
    'i64_atomic_rmw16_xor_u', 'i64_atomic_rmw32_xor_u', 'i32_atomic_rmw_xchg', 'i64_atomic_rmw_xchg',
    'i32_atomic_rmw8_xchg_u', 'i32_atomic_rmw16_xchg_u', 'i64_atomic_rmw8_xchg_u',
    'i64_atomic_rmw16_xchg_u', 'i64_atomic_rmw32_xchg_u', 'i32_atomic_rmw_cmpxchg',
    'i64_atomic_rmw_cmpxchg', 'i32_atomic_rmw8_cmpxchg_u', 'i32_atomic_rmw16_cmpxchg_u',
    'i64_atomic_rmw8_cmpxchg_u', 'i64_atomic_rmw16_cmpxchg_u', 'i64_atomic_rmw32_cmpxchg_u']

stop_commands = lldb.SBStringList()
stop_commands.AppendString("ipint_state")

go_commands = lldb.SBStringList()
go_commands.AppendString("ipint_state")
go_commands.AppendString("c")

breakpoints = []
breakpoints_enabled = []
instruction_locs = []


def extract_gprs(top_frame):
    """Given the top frame of a stack, produce a dictionary mapping register names such as 'x29' to their values."""
    regs = top_frame.GetRegisters()
    gprs = {}
    for reg in regs[0]:
        if reg.value is not None:
            gprs[reg.name] = int(reg.value[2:], 16)
    return gprs


def print_value(mem, output, prefix=None):
    raw = ' '.join(f'{x:02x}' for x in mem)
    i32 = struct.unpack('ixxxxxxxxxxxx', mem)[0]
    f32 = struct.unpack('fxxxxxxxxxxxx', mem)[0]
    i64 = struct.unpack('qxxxxxxxx', mem)[0]
    f64 = struct.unpack('dxxxxxxxx', mem)[0]
    interpretations = f'{DIM}i32:{RESET}{i32}  {DIM}f32:{RESET}{f32}  {DIM}i64{RESET}:{i64}  {DIM}f64{RESET}:{f64}'
    print(f'{prefix}{CYAN}{raw}{RESET}  {interpretations}', file=output)


def print_stack(proc, frame, pl, output, limit=100000):
    ptr = frame.sp
    slot_count = (pl - ptr) // 16
    slot_text = 'empty' if slot_count == 0 else '1 entry' if slot_count == 1 else f'{slot_count} entries'

    print(f'Stack: {DIM}({slot_text}){RESET}', file=output)
    i = 0
    while ptr != pl and i < limit:
        error = lldb.SBError()
        mem = proc.ReadMemory(ptr, 16, error)
        if error.Success():
            mem = bytearray(mem)
            print_value(mem, output, "  ")
        else:
            print(f'{RED}can\'t read stack memory at address 0x{ptr:016x} :({RESET}', file=output)
            break
        ptr += 16
        i += 1


def find_instruction_addresses(target):
    result = []
    for instr in IPINT_INSTRUCTIONS:
        contexts = target.FindSymbols(f'ipint_{instr}')
        if contexts.GetSize() == 0:
            print(f"ABORTING at ipint_{instr}")
            return []
        addr = contexts[0].GetSymbol().GetStartAddress().GetLoadAddress(target)
        if addr == 0xffffffffffffffff:
            # unresolved, let's try again later
            return []
        result.append(addr)
    return result


def find_instruction(pc, target):
    global instruction_locs
    if not instruction_locs:
        instruction_locs = find_instruction_addresses(target)
        if not instruction_locs:  # still empty, not resolved yet
            return -1
    return bisect.bisect(instruction_locs, pc) - 1


def ipint_state(debugger, command, exe_ctx, output, internal_dict):
    target = debugger.GetSelectedTarget()
    proc = target.process
    thread = proc.GetSelectedThread()
    top_frame = thread.frame[0]
    this_frame = thread.GetSelectedFrame()
    gprs = extract_gprs(top_frame)

    print('--------------- IPInt state ---------------', file=output)

    # current PC
    native_pc = this_frame.pc
    instr_index = find_instruction(native_pc, target)
    if instr_index < 0:
        print(f'Instruction = {RED}<none>{RESET} (not in IPInt)', file=output)
    else:
        print(f'Instruction = {BOLD}{IPINT_INSTRUCTIONS[instr_index]}{RESET}', file=output)

    # PC = x26
    pc = gprs[PC_REG]
    mc = gprs[MC_REG]
    pl = gprs[PL_REG]

    # preview 16 bytes of PC
    if True:
        error = lldb.SBError()
        mem = proc.ReadMemory(pc, 16, error)
        if error.Success():
            pc_data = ' '.join(f'{x:02x}' for x in mem)
        else:
            pc_data = '???'
        print(f'PC = 0x{pc:x} -> {CYAN}{pc_data}{RESET}', file=output)

    if mc != 0:
        # preview 16 bytes of MC
        if True:
            error = lldb.SBError()
            mem = proc.ReadMemory(mc, 16, error)
            if error.Success():
                mc_data = ' '.join(f'{x:02x}' for x in mem)
            else:
                mc_data = '???'
        print(f'MC = 0x{mc:x} -> {CYAN}{mc_data}{RESET}', file=output)
    else:
        print(f'MC = {RED}<none>{RESET} (no metadata generated)', file=output)

    if instr_index < 0:
        print("Stack unknown: not in IPInt", file=output)
    else:
        print_stack(proc, this_frame, pl, output)
    print('-------------------------------------------', file=output)


def ipint_stack(debugger, command, exec_ctx, output, internal_dict):
    proc = debugger.GetSelectedTarget().process
    top_frame = proc.GetSelectedThread().frame[0]
    gprs = extract_gprs(top_frame)
    pl = gprs[PL_REG]
    if not command:
        limit = 100000
    else:
        try:
            limit = int(command)
        except ValueError:
            print('usage: ipint_stack <num_stack_entries>', file=output)
            return
    print_stack(proc, top_frame, pl, output, limit)


def ipint_local(debugger, command, exec_ctx, output, internal_dict):
    try:
        local_index = int(command)
    except ValueError:
        print('usage: ipint_local <local_idx>', file=output)
        return

    proc = debugger.GetTargetAtIndex(0).process
    frame = proc.selected_thread.frame[0]

    gprs = {}
    for reg in frame.regs[0]:
        gprs[reg.name] = int(reg.value[2:], 16)

    pl = gprs[PL_REG]
    LOCAL_SIZE = 16
    error = lldb.SBError()
    ptr = pl + LOCAL_SIZE * local_index
    mem = proc.ReadMemory(ptr, 16, error)
    if error.Success():
        mem = bytearray(mem)
        print_value(mem, output)
    else:
        print(f'can\'t read stack memory at address 0x{ptr:016x} (bad local index?)', file=output)


def ipint_continue_until(debugger, command, exec_ctx, output, internal_dict):
    try:
        op_index = IPINT_INSTRUCTIONS.index(command)
        for i in range(len(IPINT_INSTRUCTIONS)):
            breakpoints[i].enabled = (i == op_index)

        proc = debugger.GetTargetAtIndex(0).process
        proc.Continue()

        for i in range(len(IPINT_INSTRUCTIONS)):
            breakpoints[i].enabled = True
    except ValueError:
        print(f'can\'t find operation {command}', file=output)


def set_breakpoints_internal(debugger, initially_enable):
    target = debugger.GetSelectedTarget()
    for instr in IPINT_INSTRUCTIONS:
        brk = target.BreakpointCreateByName(f'ipint_{instr}')
        brk.SetCommandLineCommands(stop_commands)
        brk.enabled = initially_enable
        breakpoints.append(brk)


def ipint_break_at(debugger, command, exec_ctx, output, internal_dict):
    if not breakpoints:
        set_breakpoints_internal(debugger, False)
    try:
        op_index = IPINT_INSTRUCTIONS.index(command)
        breakpoints[op_index].enabled = True
    except ValueError:
        print(f'can\'t find operation {command}', file=output)


def ipint_disable_all_breakpoints(debugger, command, exec_ctx, output, internal_dict):
    for brk in breakpoints:
        brk.enabled = False


def ipint_reenable_all_breakpoints(debugger, command, exec_ctx, output, internal_dict):
    for brk in breakpoints:
        brk.enabled = True


def ipint_continue_on_all_breakpoints(debugger, command, exec_ctx, output, internal_dict):
    for brk in breakpoints:
        brk.enabled = True
        brk.SetAutoContinue(True)


def ipint_set_all_breakpoints(debugger, command, exe_ctx, output, internal_dict):
    if not breakpoints:
        print("Initializing internal breakpoints...", file=output)
        set_breakpoints_internal(debugger, True)
        print("done!", file=output)
    else:
        print("Breakpoints already set, enabling all", file=output)
        ipint_reenable_all_breakpoints(debugger, command, exe_ctx, output, internal_dict)


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f debug_ipint.ipint_state ipint_state')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_stack ipint_stack')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_local ipint_local')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_continue_until ipint_continue_until')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_break_at ipint_break_at')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_disable_all_breakpoints ipint_disable_all_breakpoints')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_reenable_all_breakpoints ipint_reenable_all_breakpoints')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_continue_on_all_breakpoints ipint_autocontinue')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_set_all_breakpoints ipint_set_all_breakpoints')
    print("IPInt debugger ready")
