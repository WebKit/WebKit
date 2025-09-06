# WebAssembly Debugger for JavaScriptCore

A comprehensive debugging solution that enables LLDB debugging of WebAssembly code running in JavaScriptCore's IPInt (In-Place Interpreter) tier through the GDB Remote Serial Protocol.

## What is this project?

This project implements a **WebAssembly debugger server** that bridges the gap between LLDB (the LLVM debugger) and WebAssembly code execution in JavaScriptCore. It allows developers to:

- **Set breakpoints** in WebAssembly functions
- **Step through WebAssembly bytecode** instruction by instruction
- **Inspect WebAssembly locals, globals, and memory**
- **View call stacks** across WebAssembly function calls
- **Disassemble WebAssembly bytecode** in real-time
- **Use standard LLDB commands**: `continue`, `process interrupt`, `breakpoint set`, `memory read`, `register read`, `target modules list`

The implementation follows the **GDB Remote Serial Protocol** standard, making it compatible with LLDB without requiring custom extensions.

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
│  │Execution Handler│  │ Memory Handler  │  │Register Handler │  │
│  │(Breakpoints)    │  │ (WASM Memory)   │  │(Pseudo-Regs)    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ Query Handler   │  │ Module Manager  │  │Breakpoint Mgr   │  │
│  │(Capabilities)   │  │ (Virtual Addrs) │  │(Helper Class)   │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────┬───────────────────────────────────┘
                              │ Module Tracking & Execution Hooks
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                JavaScriptCore WebAssembly Engine                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │JSWebAssemblyMod │  │ IPInt Execution │  │ Next Instruction│  │
│  │(Module Tracking)│  │ (Interpreter)   │  │ (PC-MC Mapping) │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

#### 1. **WasmDebugServer** - Main Debug Server

- **Location**: `WasmDebugServer.h/cpp`
- **Purpose**: Central coordinator implementing GDB Remote Protocol
- **Key Features**:
  - TCP socket server (default port 1234)
  - Multi-threaded client handling
  - Protocol packet parsing and response generation
  - Contains all protocol handlers and helper classes

#### 2. **Protocol Handlers** - GDB Command Processing (Helper Classes)

- **WasmExecutionHandler**: Breakpoints, continue, step, interrupt
- **WasmMemoryHandler**: Memory read/write operations
- **WasmQueryHandler**: Capability negotiation and queries

#### 3. **Helper Classes** - Supporting Components

- **ModuleManager**: Virtual address space management
  - Virtual address allocation (base: `0x4000000000000000`)
  - Module spacing (4GB per module: `0x100000000`)
  - XML library list generation for LLDB
- **WasmBreakpointManager**: Breakpoint storage and management
  - Regular and temporary breakpoint support
  - Address-based breakpoint storage
  - Breakpoint hit detection and reporting

#### 4. **IPIntNextInstruction** - PC-MC Mapping Engine

- **Location**: `IPIntNextInstruction.h/cpp`
- **Purpose**: Calculate next instruction for stepping and breakpoints
- **Key Features**:
  - Handles all 200+ WebAssembly opcodes
  - Variable-length instruction parsing
  - Metadata counter (MC) advancement
  - Control flow analysis (branches, loops, calls)

### Virtual Address Space Design

```txt
Module 0: 0x4000000000000000 - 0x40000000FFFFFFFF (4GB)
├── WASM Binary: 0x4000000000000000 + offset
├── Function 0:  0x4000000000001000 
├── Function 1:  0x4000000000001100
└── ...

Module 1: 0x4000000100000000 - 0x40000001FFFFFFFF (4GB)  
├── WASM Binary: 0x4000000100000000 + offset
├── Function 0:  0x4000000100001000
└── ...
```

**Address Calculation**:

- **Module Base**: `0x4000000000000000 + (moduleIndex * 0x100000000)`
- **Function Address**: `moduleBase + functionOffset`
- **Memory Layout**: Each module gets 4GB of virtual address space

### Protocol Implementation

#### Execution

- **[DONE]** Attach to local gdb-remote with `gdb-remote localhost:1234`
- **[DONE]** `process interrupt`, `ctrl+C`: Interrupt to stop world at WASM function entry
- **[DONE]** `continue`: Continue WASM code execution
- **[DONE]** `breakpoint set`: Set breakpoints at virtual addresses in WASM functions
- **[DONE]** `step over`: Step over function calls, executing them without entering
- **[DONE]** `step in`: Step into function calls to debug inside called functions
- **[DONE]** `step out`: Step out of current function to return to caller
- **[DONE]** `step instruction`: Single step forward through WASM bytecode instruction by instruction

#### Inspection

- **[DONE]** `target modules list`: List loaded WASM modules with virtual addresses
- **[DONE]** `list`: Display WASM source lines from source files
- **[DONE]** `disassemble`: Display WASM bytecode disassembly
- **[DONE]** `bt` (backtrace): List WASM call stack (// TODO: check with DT how to show stripe stacks)
- **[DONE]** `frame variable`: List source locals
- **[DONE]** `memory region --all`: List all memory regions
- **[DONE]** `memory read`: 



## TODO and FIXME Items

### IPIntNextInstruction - Instruction Handling Issues

- Double check and fix all control flow opcodes with correct computation for the next instruction

### WasmBreakpointManager and ModuleManager - Thread Safety

- Evaluate if breakpoint and module manager needs thread synchronization locks

### Stop World - Multi-mutators

- The current `StopWorld` only works for single mutators.

### test-wasm-debugger

- Add more tests.
- Support concurrent testing.

## Build

```cli
./Tools/Scripts/build-jsc --debug
```

## Regression Test

```cli
python3 Source/JavaScriptCore/wasm/debugger/test/test-wasm-debugger.py
```

## Manual Test

Terminal 1

```cli
cd Source/JavaScriptCore/wasm/debugger/test
VM=<Path-To-WebKitBuild>/Debug && DYLD_FRAMEWORK_PATH=$VM lldb $VM/jsc -- --verboseWasmDebugger=1 --wasm-debug --useConcurrentJIT=0 test.js
```

Terminal 2

```cli
lldb -o 'log enable gdb-remote packets' -o 'gdb-remote localhost:1234'
```
