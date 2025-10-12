# WebAssembly Debugger for JavaScriptCore

A comprehensive debugging solution that enables LLDB debugging of WebAssembly code running in JavaScriptCore's IPInt (In-Place Interpreter) tier through the GDB Remote Serial Protocol.

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
- **Key Features**:
  - TCP socket server (default port 1234)
  - Single client connection handling
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
- **[DONE]** `step in`: Step into function calls
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

### Automated Tests

```bash
# Run WebAssembly debugger unit tests
./Tools/Scripts/run-javascriptcore-tests --testwasmdebugger

# Run comprehensive test framework with LLDB and wasm debugger
python3 JSTests/wasm/debugger/test-wasm-debugger.py
```

The `JSTests/wasm/debugger` includes a comprehensive test framework with auto-discovery, parallel execution, and process isolation capabilities. For details, see [JSTests/wasm/debugger/README.md](../../../../JSTests/wasm/debugger/README.md).

### Manual Testing

Terminal 1 - Start JSC with debugger:

```bash
cd JSTests/wasm/debugger/resources/add
VM=<Path-To-WebKitBuild>/Debug && DYLD_FRAMEWORK_PATH=$VM lldb $VM/jsc -- --verboseWasmDebugger=1 --wasm-debug --useConcurrentJIT=0 main.js
```

Terminal 2 - Connect LLDB:

```bash
lldb -o 'log enable gdb-remote packets' -o 'process connect --plugin wasm connect://localhost:1234'
```

## Known Issues and Future Improvements

### External IDE Debugging Support (WebContent)

- **Issue**: Current implementation only works with JSC shell, not WebContent processes
- **Goal**: Enable debugging WASM using external IDEs (VS Code, etc.) via LLDB
- **Architecture**:
  ```txt
  External IDE ←→ LLDB ←→ RWI Bridge App ←→ webinspectord ←→ WebContent
  ```
- **Key Components Needed**:
  - Add WebAssembly debuggable type to RWI system
  - Create WASM RWI target registration in WebContent process
  - Implement RWI message protocol for WASM debugging
  - Create external RWI bridge application (LLDB ↔ RWI translator)
  - Integrate existing debug server with RWI bridge

### Multi-VM Debugging Support

- **Issue**: Current implementation only stops a single VM when hitting breakpoints
- **Location**: `WasmExecutionHandler.cpp:65-66`
- **Solution**: When ANY VM hits a WebAssembly breakpoint, stop ALL execution across ALL VMs in the process for comprehensive debugging

### WASM Stack Value Type Support

- **Issue**: Current implementation only supports WASM local variable inspection, missing WASM stack value types
- **Current Support**: Local variables with types (parameters and locals in function scope)
- **Missing Support**: Stack values with types
- **Solution**: Extend debugging protocol to expose WASM operand stack contents with proper type information
- **Benefits**: Complete variable inspection during debugging, better understanding of WASM execution state

### Step Into Call Instructions

- **Issue**: Step into breakpoints not implemented for CallIndirect instructions
- **Location**: `WasmExecutionHandler.cpp:298-299`
- **Solution**: Add step into breakpoint support for indirect function calls

### Control Flow Debug Info Validation

- **Issue**: Not all WebAssembly control flow bytecodes may have correct debug info collection for next instruction mappings
- **Location**: `WasmIPIntGenerator.cpp` - Various control flow instruction handlers
- **Solution**: Systematically test each control flow bytecode (block, loop, if, try, catch, br, br_if, br_table, call, call_indirect, return, etc.) to ensure corresponding debug info is collected correctly for accurate stepping behavior

### Client Session Management

- **Issue**: Client disconnect, kill, and quit commands only stop the client session for debugging purposes
- **Location**: `WasmDebugServer.cpp:348-349`
- **Solution**: Introduce various stop states and proper termination handling

### Dynamic Module Notifications

- **Issue**: LLDB is not notified when new modules are loaded or unloaded
- **Location**: `WasmDebugServer.cpp:472, 484`
- **Solution**: Implement proper LLDB notifications for dynamic module loading/unloading

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
