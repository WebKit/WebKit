
# IPInt debugger scripts for looking at the state of the program
# Run with:
# lldb -s debug_ipint.lldb -- jsc (args)

import lldb

import bisect
import struct
from pathlib import Path

ARCH = 'arm64'

if ARCH == 'arm64':
    PC_REG = 'x26'
    MC_REG = 'x25'
    PL_REG = 'x6'

# Read all instruction opcodes from InPlaceInterpreter64.asm

IPINT_INSTRUCTIONS = [
    'unreachable', 'nop', 'block', 'loop', 'if', 'else', 'try', 'catch', 'throw', 'rethrow', 'end', 'br', 'br_if',
    'br_table', 'return', 'call', 'call_indirect', 'delegate', 'catch_all', 'drop', 'select', 'select_t', 'local_get',
    'local_set', 'local_tee', 'global_get', 'global_set', 'table_get', 'table_set', 'i32_load_mem', 'i64_load_mem',
    'f32_load_mem', 'f64_load_mem', 'i32_load8s_mem', 'i32_load8u_mem', 'i32_load16s_mem', 'i32_load16u_mem',
    'i64_load8s_mem', 'i64_load8u_mem', 'i64_load16s_mem', 'i64_load16u_mem', 'i64_load32s_mem', 'i64_load32u_mem',
    'i32_store_mem', 'i64_store_mem', 'f32_store_mem', 'f64_store_mem', 'i32_store8_mem', 'i32_store16_mem',
    'i64_store8_mem', 'i64_store16_mem', 'i64_store32_mem', 'memory_size', 'memory_grow', 'i32_const', 'i64_const',
    'f32_const', 'f64_const', 'i32_eqz', 'i32_eq', 'i32_ne', 'i32_lt_s', 'i32_lt_u', 'i32_gt_s', 'i32_gt_u', 'i32_le_s',
    'i32_le_u', 'i32_ge_s', 'i32_ge_u', 'i64_eqz', 'i64_eq', 'i64_ne', 'i64_lt_s', 'i64_lt_u', 'i64_gt_s', 'i64_gt_u',
    'i64_le_s', 'i64_le_u', 'i64_ge_s', 'i64_ge_u', 'f32_eq', 'f32_ne', 'f32_lt', 'f32_gt', 'f32_le', 'f32_ge', 'f64_eq',
    'f64_ne', 'f64_lt', 'f64_gt', 'f64_le', 'f64_ge', 'i32_clz', 'i32_ctz', 'i32_popcnt', 'i32_add', 'i32_sub',
    'i32_mul', 'i32_div_s', 'i32_div_u', 'i32_rem_s', 'i32_rem_u', 'i32_and', 'i32_or', 'i32_xor', 'i32_shl',
    'i32_shr_s', 'i32_shr_u', 'i32_rotl', 'i32_rotr', 'i64_clz', 'i64_ctz', 'i64_popcnt', 'i64_add', 'i64_sub',
    'i64_mul', 'i64_div_s', 'i64_div_u', 'i64_rem_s', 'i64_rem_u', 'i64_and', 'i64_or', 'i64_xor', 'i64_shl',
    'i64_shr_s', 'i64_shr_u', 'i64_rotl', 'i64_rotr', 'f32_abs', 'f32_neg', 'f32_ceil', 'f32_floor', 'f32_trunc',
    'f32_nearest', 'f32_sqrt', 'f32_add', 'f32_sub', 'f32_mul', 'f32_div', 'f32_min', 'f32_max', 'f32_copysign',
    'f64_abs', 'f64_neg', 'f64_ceil', 'f64_floor', 'f64_trunc', 'f64_nearest', 'f64_sqrt', 'f64_add', 'f64_sub',
    'f64_mul', 'f64_div', 'f64_min', 'f64_max', 'f64_copysign', 'i32_wrap_i64', 'i32_trunc_f32_s', 'i32_trunc_f32_u',
    'i32_trunc_f64_s', 'i32_trunc_f64_u', 'i64_extend_i32_s', 'i64_extend_i32_u', 'i64_trunc_f32_s', 'i64_trunc_f32_u',
    'i64_trunc_f64_s', 'i64_trunc_f64_u', 'f32_convert_i32_s', 'f32_convert_i32_u', 'f32_convert_i64_s',
    'f32_convert_i64_u', 'f32_demote_f64', 'f64_convert_i32_s', 'f64_convert_i32_u', 'f64_convert_i64_s',
    'f64_convert_i64_u', 'f64_promote_f32', 'i32_reinterpret_f32', 'i64_reinterpret_f64', 'f32_reinterpret_i32',
    'f64_reinterpret_i64', 'i32_extend8_s', 'i32_extend16_s', 'i64_extend8_s', 'i64_extend16_s', 'i64_extend32_s',
    'ref_null_t', 'ref_is_null', 'ref_func', 'fc_block', 'simd', 'atomic', 'i32_trunc_sat_f32_s', 'i32_trunc_sat_f32_u',
    'i32_trunc_sat_f64_s', 'i32_trunc_sat_f64_u', 'i64_trunc_sat_f32_s', 'i64_trunc_sat_f32_u', 'i64_trunc_sat_f64_s',
    'i64_trunc_sat_f64_u', 'memory_init', 'data_drop', 'memory_copy', 'memory_fill', 'table_init', 'elem_drop',
    'table_copy', 'table_grow', 'table_size', 'table_fill', 'simd_v128_const', 'simd_i32x4_extract_lane',
    'memory_atomic_notify', 'memory_atomic_wait32', 'memory_atomic_wait64', 'atomic_fence', 'i32_atomic_load',
    'i64_atomic_load', 'i32_atomic_load8_u', 'i32_atomic_load16_u', 'i64_atomic_load8_u', 'i64_atomic_load16_u',
    'i64_atomic_load32_u', 'i32_atomic_store', 'i64_atomic_store', 'i32_atomic_store8_u', 'i32_atomic_store16_u',
    'i64_atomic_store8_u', 'i64_atomic_store16_u', 'i64_atomic_store32_u', 'i32_atomic_rmw_add', 'i64_atomic_rmw_add',
    'i32_atomic_rmw8_add_u', 'i32_atomic_rmw16_add_u', 'i64_atomic_rmw8_add_u', 'i64_atomic_rmw16_add_u',
    'i64_atomic_rmw32_add_u', 'i32_atomic_rmw_sub', 'i64_atomic_rmw_sub', 'i32_atomic_rmw8_sub_u',
    'i32_atomic_rmw16_sub_u', 'i64_atomic_rmw8_sub_u', 'i64_atomic_rmw16_sub_u', 'i64_atomic_rmw32_sub_u',
    'i32_atomic_rmw_and', 'i64_atomic_rmw_and', 'i32_atomic_rmw8_and_u', 'i32_atomic_rmw16_and_u',
    'i64_atomic_rmw8_and_u', 'i64_atomic_rmw16_and_u', 'i64_atomic_rmw32_and_u', 'i32_atomic_rmw_or',
    'i64_atomic_rmw_or', 'i32_atomic_rmw8_or_u', 'i32_atomic_rmw16_or_u', 'i64_atomic_rmw8_or_u', 'i64_atomic_rmw16_or_u',
    'i64_atomic_rmw32_or_u', 'i32_atomic_rmw_xor', 'i64_atomic_rmw_xor', 'i32_atomic_rmw8_xor_u',
    'i32_atomic_rmw16_xor_u', 'i64_atomic_rmw8_xor_u', 'i64_atomic_rmw16_xor_u', 'i64_atomic_rmw32_xor_u',
    'i32_atomic_rmw_xchg', 'i64_atomic_rmw_xchg', 'i32_atomic_rmw8_xchg_u', 'i32_atomic_rmw16_xchg_u',
    'i64_atomic_rmw8_xchg_u', 'i64_atomic_rmw16_xchg_u', 'i64_atomic_rmw32_xchg_u', 'i32_atomic_rmw_cmpxchg',
    'i64_atomic_rmw_cmpxchg', 'i32_atomic_rmw8_cmpxchg_u', 'i32_atomic_rmw16_cmpxchg_u', 'i64_atomic_rmw8_cmpxchg_u',
    'i64_atomic_rmw16_cmpxchg_u', 'i64_atomic_rmw32_cmpxchg_u']

stop_commands = lldb.SBStringList()
stop_commands.AppendString("ipint_state")

go_commands = lldb.SBStringList()
go_commands.AppendString("ipint_state")
go_commands.AppendString("c")

breakpoints = []
breakpoints_enabled = []
breakpoint_locs = []


def print_value(mem, result):
    print(' '.join(f'{x:02x}' for x in mem), file=result)
    # interpret as a bunch of stuff
    SP = ' ' * 10
    print(f'{SP}\033[92mi32\033[0m{SP}{SP}\033[93mf32\033[0m{SP}{SP}\033[94mi64\033[0m{SP}{SP}\033[95mf64\033[0m', file=result)
    i32 = struct.unpack('ixxxxxxxxxxxx', mem)[0]
    f32 = struct.unpack('fxxxxxxxxxxxx', mem)[0]
    i64 = struct.unpack('qxxxxxxxx', mem)[0]
    f64 = struct.unpack('dxxxxxxxx', mem)[0]
    print(f'{i32:^23}{f32:^23.5f}{i64:^23}{f64:^23.5f}', file=result)


def print_stack(proc, frame, result, num_entries=None):
    gprs = {}
    for reg in frame.regs[0]:
        gprs[reg.name] = int(reg.value[2:], 16)
    pl = gprs[PL_REG]

    ptr = frame.sp
    i = 0

    if ptr == pl:
        print('no stack entries', file=result)
    else:
        print('(top of stack)', file=result)

    while ptr != pl and (num_entries is None or i < num_entries):
        print(f'\n---- stack[{i:2}] ----', file=result)
        error = lldb.SBError()
        mem = proc.ReadMemory(ptr, 16, error)
        if error.Success():
            mem = bytearray(mem)
            print_value(mem, result)
        else:
            print(f'can\'t read stack memory at address 0x{ptr:016x} :(', file=result)
            break
        ptr += 16
        i += 1


def ipint_state(debugger, command, exe_ctx, result, internal_dict):
    print("Current interpreter state:", file=result)
    print("--------------------------", file=result)

    proc = debugger.GetTargetAtIndex(0).process
    frame = proc.selected_thread.frame[0]
    regs = frame.regs

    # Figure out where we are if breakpoints is populated
    if len(breakpoints) != 0:
        # current PC
        pc = frame.pc
        i = bisect.bisect(breakpoint_locs, pc) - 1
        if i < 0:
            print(f'\033[1m\033[91mnot in IPInt bounds!!!\033[0m')
        else:
            print(f'\033[1mCurrently executing: \033[93m{IPINT_INSTRUCTIONS[i]}\033[0m', file=result)

    gprs = {}
    for reg in regs[0]:
        gprs[reg.name] = int(reg.value[2:], 16)

    # PC = x26
    pc = gprs[PC_REG]
    mc = gprs[MC_REG]

    print(f'PC = 0x{pc:x}', file=result)

    # preview 8 bytes of PC
    if True:
        error = lldb.SBError()
        mem = proc.ReadMemory(pc, 8, error)
        if error.Success():
            print(' '.join(f'{x:02x}' for x in mem), file=result)
        else:
            print('???', file=result)
    print('', file=result)

    if mc != 0:
        print(f'MC = 0x{mc:x}', file=result)

        # preview 8 bytes of MC
        if True:
            error = lldb.SBError()
            mem = proc.ReadMemory(mc, 8, error)
            if error.Success():
                print(' '.join(f'{x:02x}' for x in mem), file=result)
            else:
                print('???', file=result)
        print('', file=result)
    else:
        print('\033[1m\033[94mno metadata generated\033[0m\n', file=result)

    print_stack(proc, frame, result, 2)


def ipint_stack(debugger, command, exec_ctx, result, internal_dict):
    proc = debugger.GetTargetAtIndex(0).process
    frame = proc.selected_thread.frame[0]
    try:
        print_stack(proc, frame, result, int(command))
    except ValueError:
        print('usage: ipint_stack <num_stack_entries>', file=result)


def ipint_local(debugger, command, exec_ctx, result, internal_dict):
    try:
        local_index = int(command)
    except ValueError:
        print('usage: ipint_local <local_idx>', file=result)
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
        print_value(mem, result)
    else:
        print(f'can\'t read stack memory at address 0x{ptr:016x} (bad local index?)', file=result)


def ipint_continue_until(debugger, command, exec_ctx, result, internal_dict):
    try:
        op_index = IPINT_INSTRUCTIONS.index(command)
        for i in range(len(IPINT_INSTRUCTIONS)):
            breakpoints[i].enabled = (i == op_index)

        proc = debugger.GetTargetAtIndex(0).process
        proc.Continue()

        for i in range(len(IPINT_INSTRUCTIONS)):
            breakpoints[i].enabled = True
    except ValueError:
        print(f'can\'t find operation {command}', file=result)


def ipint_break_at(debugger, command, exec_ctx, result, internal_dict):
    try:
        op_index = IPINT_INSTRUCTIONS.index(command)
        breakpoints[op_index].enabled = True
    except ValueError:
        print(f'can\'t find operation {command}', file=result)


def ipint_disable_all_breakpoints(debugger, command, exec_ctx, result, internal_dict):
    for brk in breakpoints:
        brk.enabled = False


def ipint_reenable_all_breakpoints(debugger, command, exec_ctx, result, internal_dict):
    for brk in breakpoints:
        brk.enabled = True


def ipint_continue_on_all_breakpoints(debugger, command, exec_ctx, result, internal_dict):
    for brk in breakpoints:
        brk.enabled = True
        brk.SetAutoContinue(True)


def set_breakpoints(debugger, command, exe_ctx, result, internal_dict):
    print("Initializing internal breakpoints...", file=result)
    target = debugger.GetTargetAtIndex(0)

    for instr in IPINT_INSTRUCTIONS:
        brk = target.BreakpointCreateByName(f'ipint_{instr}')
        brk.SetCommandLineCommands(stop_commands)
        breakpoints.append(brk)
        breakpoint_locs.append(brk.locations[0].GetLoadAddress())
    print("done!", file=result)


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f debug_ipint.ipint_state ipint_state')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_stack ipint_stack')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_local ipint_local')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_continue_until ipint_continue_until')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_break_at ipint_break_at')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_disable_all_breakpoints ipint_disable_all_breakpoints')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_reenable_all_breakpoints ipint_reenable_all_breakpoints')
    debugger.HandleCommand('command script add -f debug_ipint.ipint_continue_on_all_breakpoints ipint_autocontinue')
    debugger.HandleCommand('command script add -f debug_ipint.set_breakpoints set_breakpoints')
    print("IPInt debugger ready")
