# WebAssembly Debugger for JavaScriptCore

A comprehensive debugging solution that enables LLDB debugging of WebAssembly code running in JavaScriptCore's IPInt (In-Place Interpreter) tier through the GDB Remote Serial Protocol.

> **Related Documentation:**
>
> - **This document**: JSC debug server implementation (both Standalone and RWI modes)
> - **[RWI_ARCHITECTURE.md](./RWI_ARCHITECTURE.md)**: WebKit integration architecture (RWI mode details)
> - **[Debugger-Mutator-Protocol.md](./Debugger-Mutator-Protocol.md)**: Thread synchronization protocol and control flow diagrams

## What is this project?

This project implements a **WebAssembly debugger server** that bridges the gap between LLDB (the LLVM debugger) and WebAssembly code execution in JavaScriptCore. It allows developers to:

- **Set breakpoints** in WebAssembly functions
- **Step through WebAssembly bytecode** instruction by instruction
- **Inspect WebAssembly locals, globals, and memory**
- **View call stacks** across WebAssembly function calls
- **Disassemble WebAssembly bytecode** in real-time

The implementation follows the **GDB Remote Serial Protocol** standard with [wasm extension](https://lldb.llvm.org/resources/lldbgdbremote.html#wasm-packets).

## Design and Architecture

### High-Level Architecture

```txt
┌─────────────────────────────────────────────────────────────────┐
│                        LLDB Debugger                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   Breakpoints   │  │  Symbol Lookup  │  │ Execution Ctrl  │  │
│  │   Management    │  │   & Modules     │  │   & Stepping    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────┬───────────────────────────────────┘
                              │ GDB Remote Protocol (TCP:1234)
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                      WasmDebugServer                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │Execution Handler│  │ Memory Handler  │  │ Query Handler   │  │
│  │(Breakpoints)    │  │ (WASM Memory)   │  │(Capabilities)   │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ Module Manager  │  │Breakpoint Mgr   │  │                 │  │
│  │ (Virtual Addrs) │  │(Helper Class)   │  │                 │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────┬───────────────────────────────────┘
                              │ Module Tracking & Execution Hooks
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                JavaScriptCore WebAssembly Engine                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │JSWebAssemblyMod │  │ IPInt Execution │  │ Debug Info      │  │
│  │(Module Tracking)│  │ (Interpreter)   │  │ (PC Mapping)    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

#### 1. **WasmDebugServer** - Main Debug Server

- **Location**: `WasmDebugServer.h/cpp`
- **Purpose**: Central coordinator implementing GDB Remote Protocol
- **Two Modes**:
  - **Standalone Mode**: TCP socket server (default port 1234) for JSC shell debugging
  - **RWI Mode**: IPC-based communication for WebKit/WebContent debugging (see [RWI_ARCHITECTURE.md](./RWI_ARCHITECTURE.md))
- **Key Features**:
  - Protocol packet parsing and response generation
  - Contains all protocol handlers and helper classes

#### 2. **Protocol Handlers** - GDB Command Processing

- **ExecutionHandler**: Breakpoints, continue, step, interrupt
- **MemoryHandler**: Memory read/write operations
- **QueryHandler**: Capability negotiation and queries

#### 3. **Helper Classes** - Supporting Components

- **ModuleManager**: Virtual address space management and module tracking
- **BreakpointManager**: Breakpoint storage and management
- **VirtualAddress**: 64-bit virtual address encoding for LLDB compatibility

### Virtual Address Space Design

The debugger uses a sophisticated virtual address encoding system to present WebAssembly modules and memory to LLDB:

```txt
Address Format (64-bit):
- Bits 63-62: Address Type (2 bits)
- Bits 61-32: ID (30 bits) - ModuleID for code, InstanceID for memory  
- Bits 31-0:  Offset (32 bits)

Address Types:
- 0x00 (Memory): Instance linear memory
- 0x01 (Module): Module code/bytecode
- 0x02 (Invalid): Invalid/unmapped regions
- 0x03 (Invalid2): Invalid/unmapped regions

Virtual Memory Layout:
- 0x0000000000000000 - 0x3FFFFFFFFFFFFFFF: Memory regions
- 0x4000000000000000 - 0x7FFFFFFFFFFFFFFF: Module regions  
- 0x8000000000000000 - 0xFFFFFFFFFFFFFFFF: Invalid regions
```

## Protocol Implementation

### Execution Control

- **[DONE]** `gdb-remote localhost:1234`: Attach to debugger
- **[DONE]** `process interrupt`, `ctrl+C`: Stop execution at function entry
- **[DONE]** `continue`: Resume WebAssembly execution
- **[DONE]** `breakpoint set`: Set breakpoints at virtual addresses
- **[DONE]** `step over`: Step over function calls
- **[DONE]** `step in`: Step into function calls and exception handlers
- **[DONE]** `step out`: Step out of current function
- **[DONE]** `step instruction`: Single step through bytecode

### Inspection

- **[DONE]** `target modules list`: List loaded WebAssembly modules
- **[DONE]** `disassemble`: Display WebAssembly bytecode
- **[DONE]** `bt` (backtrace): Show WebAssembly call stack
- **[DONE]** `frame variable`: List local variables
- **[DONE]** `memory region --all`: List memory regions
- **[DONE]** `memory read`: Read WebAssembly memory and module source
- **[TODO]** `memory write`: Write to memory? source? or both?

## Testing

### Unit Tests

The debugger includes comprehensive unit tests that validate debug info generation
for WebAssembly opcodes:

```bash
# Run unit tests via WebKit build system
./Tools/Scripts/run-javascriptcore-tests --testwasmdebugger
```

**Opcode Coverage (Base OpType):**

- **[DONE]** Special Ops (FOR_EACH_WASM_SPECIAL_OP)
- **[DONE]** Control Flow Ops (FOR_EACH_WASM_CONTROL_FLOW_OP)
- **[DONE]** Unary Ops (FOR_EACH_WASM_UNARY_OP)
- **[DONE]** Binary Ops (FOR_EACH_WASM_BINARY_OP)
- **[DONE]** Memory Load Ops (FOR_EACH_WASM_MEMORY_LOAD_OP)
- **[DONE]** Memory Store Ops (FOR_EACH_WASM_MEMORY_STORE_OP)

**Extended Opcode Coverage:**

- **[TODO]** Ext1OpType (FOR_EACH_WASM_EXT1_OP)
- **[PARTIAL]** ExtGCOpType (FOR_EACH_WASM_GC_OP) - 2 control flow ops fully tested (BrOnCast, BrOnCastFail), 29 non-control-flow ops have stub tests
- **[TODO]** ExtAtomicOpType (FOR_EACH_WASM_EXT_ATOMIC_OP)
- **[TODO]** ExtSIMDOpType (FOR_EACH_WASM_EXT_SIMD_OP)

### Integration Tests

The `JSTests/wasm/debugger` includes a comprehensive test framework with auto-discovery,
parallel execution, and process isolation capabilities:

```bash
# Run comprehensive test framework with LLDB and wasm debugger
python3 JSTests/wasm/debugger/test-wasm-debugger.py
```

For details, see [JSTests/wasm/debugger/README.md](../../../../JSTests/wasm/debugger/README.md).

### Manual Testing

**Standalone Mode (JSC Shell):**

Terminal 1 - Start JSC with debugger:

```bash
cd JSTests/wasm/debugger/resources/add
VM=<Path-To-WebKitBuild>/Debug && DYLD_FRAMEWORK_PATH=$VM lldb $VM/jsc -- --verboseWasmDebugger=1 --wasm-debugger --useConcurrentJIT=0 main.js
```

Terminal 2 - Connect LLDB:

```bash
lldb -o 'log enable gdb-remote packets' -o 'process connect --plugin wasm connect://localhost:1234'
```

**RWI Mode (WebKit/WebContent):**

See [RWI_ARCHITECTURE.md](./RWI_ARCHITECTURE.md) for complete setup instructions including:

- Starting Safari/MiniBrowser with `--wasm-debugger` flag
- Using WasmDebuggerRWIClient to relay LLDB commands
- Debugging WebContent processes via Remote Web Inspector

## Known Issues and Future Improvements

### WASM Stack Value Type Support

- **Issue**: Current implementation only supports WASM local variable inspection, missing WASM stack value types
- **Current Support**: Local variables with types (parameters and locals in function scope)
- **Missing Support**: Stack values with types
- **Solution**: Extend debugging protocol to expose WASM operand stack contents with proper type information
- **Benefits**: Complete variable inspection during debugging, better understanding of WASM execution state

### Extended Opcode Test Coverage

- **Issue**: Current unit tests only cover base OpType opcodes; ExtGCOpType has partial coverage with stub implementations
- **Complete Coverage**:
  - ExtGCOpType control flow: BrOnCast, BrOnCastFail (fully tested)
- **Partial Coverage**:
  - ExtGCOpType non-control-flow: 29 opcodes have stub tests that need proper implementation
- **Missing Coverage**:
  - Ext1OpType (table operations, saturated truncation)
  - ExtAtomicOpType (atomic operations)
  - ExtSIMDOpType (SIMD operations)

### Client Session Management

- **Issue**: Client disconnect, kill, and quit commands only stop the client session for debugging purposes
- **Location**: `WasmDebugServer.cpp:348-349`
- **Solution**: Introduce various stop states and proper termination handling

### Dynamic Module Notifications

- **Issue**: LLDB is not notified when new modules are loaded or unloaded
- **Location**: `WasmDebugServer.cpp:472, 484`
- **Solution**: Implement proper LLDB notifications for dynamic module loading/unloading

### Multi-Thread Display in LLDB

- **Issue**: Thread select and stop reply protocol handlers need improvement to correctly display multi-VM data in LLDB
- **Current Status**: Multi-VM stop-the-world is implemented, but thread information may not display correctly in LLDB UI

### VM Lifecycle and Synchronization Testing

- **Issue**: ExecutionHandler stress tests need additional coverage for VM lifecycle edge cases and race conditions
- **Current Test Limitations**: Tests wait for VM construction and instance registration, but this doesn't guarantee VMs are actively running code that checks traps
- **Missing Test Coverage**:
  - VM lifecycle edge cases (construction, initialization, instance registration)
  - `interrupt()` race conditions when VMs are not yet executing code
  - Synchronization between VM construction and actual code execution

### X86_64 Support

- **Issue**: The WebAssembly debugger is currently restricted to ARM64 platforms only
- **Known Problems on Other Platforms**:
  - x86_64: VMTraps race condition causes register corruption during interrupt handling
  - ARM32, iOS: Untested

## Protocol References

The following references correspond to the numbered citations used throughout the WebAssembly debugger implementation:

- [1] [Interrupts](https://sourceware.org/gdb/onlinedocs/gdb/Interrupts.html)  
- [2] [Packet Acknowledgment](https://sourceware.org/gdb/onlinedocs/gdb/Packet-Acknowledgment.html)  
- [3] [Packets](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Packets.html)  
- [4] [General Query Packets](https://sourceware.org/gdb/current/onlinedocs/gdb.html/General-Query-Packets.html)
- [5] [Standard Replies](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Standard-Replies.html#Standard-Replies)
- [6] [Packet Acknowledgment](https://sourceware.org/gdb/onlinedocs/gdb/Packet-Acknowledgment.html)  
- [7] [qSupported](https://sourceware.org/gdb/current/onlinedocs/gdb.html/General-Query-Packets.html#qSupported)  
- [8] [qProcessInfo](https://lldb.llvm.org/resources/lldbgdbremote.html#qprocessinfo)  
- [9] [qHostInfo](https://lldb.llvm.org/resources/lldbgdbremote.html#qhostinfo)  
- [10] [qRegisterInfo](https://lldb.llvm.org/resources/lldbgdbremote.html#qregisterinfo-hex-reg-id)  
- [11] [qListThreadsInStopReply](https://lldb.llvm.org/resources/lldbgdbremote.html#qlistthreadsinstopreply)  
- [12] [qEnableErrorStrings](https://lldb.llvm.org/resources/lldbgdbremote.html#qenableerrorstrings)  
- [13] [qThreadStopInfo](https://lldb.llvm.org/resources/lldbgdbremote.html#qthreadstopinfo-tid)  
- [14] [qXfer:library-list:read](https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html#qXfer-library-list-read)  
- [15] [qWasmCallStack](https://lldb.llvm.org/resources/lldbgdbremote.html#qwasmcallstack)  
- [16] [qWasmLocal](https://lldb.llvm.org/resources/lldbgdbremote.html#qwasmlocal)  
- [17] [qMemoryRegionInfo](https://lldb.llvm.org/resources/lldbgdbremote.html#qmemoryregioninfo-addr)
