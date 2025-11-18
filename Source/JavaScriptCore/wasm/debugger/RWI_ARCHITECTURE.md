# WebKit WebAssembly Debugging Architecture

**Process-Wide RWI Integration with WorkQueue-Based Infinite Loop Support**

> **Related Documentation:**
> - **This document**: RWI mode architecture (WebKit integration)
> - **[README.md](./README.md)**: JSC debug server implementation (both Standalone and RWI modes)

---

## Overview

WebKit's WebAssembly debugging uses a **singleton `WasmDebugServer`** that manages all Wasm modules across the entire WebContent process. This design provides a unified debugging view similar to LLDB debugging multi-threaded C++ programs.

### Key Design Principles

1. **Process-Wide Singleton**: One `WasmDebugServer::singleton()` per WebContent process
2. **WorkQueue-Based IPC**: Debugging commands processed on background thread (supports debugging infinite loops)
3. **RWI Integration**: One `WasmDebuggerDebuggable` target per WebContent process
4. **GDB Remote Serial Protocol**: LLDB communicates using standard GDB packets

### Full Stack Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│ External IDE (VS Code / CLion)                                   │
│ ↕ LLDB Protocol                                                  │
│ LLDB Client                                                      │
│ ↕ GDB Remote Serial Protocol (TCP)                               │
│ WasmDebuggerRWIClient (Relay)                                    │
│ ↕ WebInspector.framework RWI Protocol                            │
│ webinspectord (System Daemon)                                    │
│ ↕ RemoteInspector IPC                                            │
│ WasmDebuggerDebuggable (UI Process)                              │
│ ↕ WebKit IPC                                                     │
│ WasmDebuggerDispatcher (WorkQueue thread)                        │
│ ↕ Direct call                                                    │
│ WasmDebugServer (JavaScriptCore)                                 │
│ ↕ Breakpoint control                                             │
│ Wasm Execution (IPInt interpreter)                               │
└──────────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### External Client

**WasmDebuggerRWIClient** - Relay between LLDB and WebKit
- External standalone application (separate repository)
- Creates TCP server for LLDB connections (default port 9222)
- Uses WebInspector.framework to communicate with webinspectord
- Provides target discovery and connection management
- Forwards raw GDB packets bidirectionally (no protocol translation)

### UI Process (Safari)

**WebProcessProxy** - Manages one WebContent process
- Creates `WasmDebuggerDebuggable` on construction (if `--wasm-debugger` enabled)
- Forwards debugging commands via IPC
- Routes responses back to LLDB

**WasmDebuggerDebuggable** - RWI target for process
- Target ID: Auto-assigned numeric value by RemoteInspector
- Target Name: `"WebAssembly Debugger (WebContent PID {osProcessID})"` (displays actual OS PID)
- Type: `RemoteControllableTarget::Type::WasmDebugger`
- Bridges LLDB ↔ WebContent process
- Implements `RemoteInspectionTarget` interface (required by RWI framework)
- Called by RWI framework (`RemoteConnectionToTarget`) when clients connect/disconnect

### WebContent Process (WebKit)

**WasmDebuggerDispatcher** - WorkQueue-based IPC receiver
- Receives debugging commands on **WorkQueue thread** (not main thread)
- Enables debugging when main thread blocked in infinite loop
- Matches `WebInspectorInterruptDispatcher` pattern (JavaScript debugging)

**WebProcess** - Process singleton
- Owns `WasmDebuggerDispatcher` instance
- Initializes `WasmDebugServer` on startup
- Sets up response handler for IPC communication

### JavaScriptCore (JSC)

**WasmDebugServer** - Process-wide debugging coordinator
- Singleton: `DebugServer::singleton()`
- Dual-mode operation: Standalone (TCP) and RWI (IPC)
- Thread-safe in RWI mode: WorkQueue is serial, guarantees no concurrent access to `handleRawPacket()`
- Tracks all Wasm modules and instances across entire process

> **Implementation Details**: See [README.md](./README.md) for debug server internals, protocol handlers, and virtual address encoding.

---

## Communication Pipeline

**High-Level Flow:**

```
LLDB (IDE)
    ↕
WasmDebuggerRWIClient
    ↕
webinspectord
    ↕
WebProcessProxy (UI Process)
    ↕ IPC
WasmDebuggerDispatcher (WebContent Process, WorkQueue)
    ↕
WasmDebugServer (JSC)
```

**Key Points:**
- External client relays raw GDB packets between LLDB and WebKit
- Target registration: WebProcessProxy creates `WasmDebuggerDebuggable` with auto-assigned numeric Target ID and name showing OS PID
- IPC routing: Messages dispatched to WorkQueue in WebContent Process, NOT main thread
- WorkQueue enables debugging when main thread blocked in infinite loop
- Pattern matches `WebInspectorInterruptDispatcher` (JavaScript debugging)

**Critical Architecture Decision: WorkQueue Threading**

Main thread can be blocked in infinite Wasm loop, but debugging must still work:
- Kernel queues IPC messages independently
- WorkQueue thread (in WebContent Process) processes debug commands concurrently
- Mutator thread waits on condition variable when paused
- WorkQueue thread signals condition variable to resume execution

---

## RWI Integration

**Target Type:** `RWIDebuggableTypeWasmDebugger` (distinct from `RWIDebuggableTypeWebPage`)

**Key Differences from Web Inspector:**
- Web Inspector: JSON-RPC protocol, per-page lifecycle, visual indicators
- Wasm Debugger: Raw GDB packets, process-lifetime, no visual UI

**Process-Lifetime Semantics:**
Unlike Web Inspector which initializes/tears down controllers per page, WebAssembly debugging runs for the entire process lifetime. The debug server starts with the process and continues running regardless of RWI connection state.

---

## Key Implementation Files

**UI Process:**
- `UIProcess/Inspector/WasmDebuggerDebuggable.{h,cpp}` - RWI target implementation
- `UIProcess/WebProcessProxy.{h,cpp}` - IPC forwarding and lifecycle

**WebContent Process:**
- `WebProcess/Inspector/WasmDebuggerDispatcher.{h,cpp,messages.in}` - WorkQueue dispatcher
- `WebProcess/WebProcess.cpp` - Initialization and response handler

**JavaScriptCore:**
- `wasm/debugger/WasmDebugServer.{h,cpp}` - Core debug server (dual-mode: TCP/RWI)
- `wasm/debugger/WasmExecutionHandler.cpp` - GDB execution control
- `wasm/debugger/WasmModuleManager.cpp` - Module/instance tracking

**RWI Framework:**
- `inspector/remote/RemoteInspector.{h,cpp}` - Target registration
- `inspector/remote/RemoteConnectionToTarget.cpp` - Connection lifecycle
- `inspector/remote/cocoa/RemoteInspectorCocoa.mm` - Platform implementation

---

## Design Rationale

### Why Process-Wide Singleton?

- **Simplicity**: One debug server, one RWI target per process
- **Natural fit**: Matches LLDB's process-level debugging model
- **Unified view**: All modules visible in single registry
- **Shared modules**: Multiple pages can share same Wasm module bytecode
- **Easier IPC**: Single communication channel for entire process

### Why WorkQueue Instead of Main Thread?

- **Infinite loop support**: Main thread can be blocked in `while(true)`
- **Proven pattern**: Matches JavaScript Inspector (`WebInspectorInterruptDispatcher`)
- **Kernel queuing**: Mach messages queued even when process doesn't process RunLoop
- **Thread safety**: WorkQueue is serial (processes one message at a time, no concurrent access)
- **No busy-wait**: Condition variables efficiently wake blocked threads

### Why Not Per-Page Targets?

**Virtual Address Space Fragmentation** (Critical Technical Limitation):
   LLDB requires a **unified virtual address space** for debugging. With per-page targets:
   - Each page would have its own Module ID 0, creating address collisions
   - LLDB breakpoint at virtual address `0x4000000000000100` - which page does this refer to?
   - Same module in different pages would report different IDs, breaking module sharing
   - Memory inspection fails (instance IDs would overlap between pages)
   - Stack traces and disassembly become ambiguous

   Process-wide target provides:
   - Global module ID allocation (Module A = ID 0 across ALL pages)
   - Unique instance IDs (each page's instance gets unique ID)
   - Coherent virtual address space LLDB expects
   - Stable addresses for breakpoints regardless of which page uses the module

   > **Technical Details**: See [README.md](./README.md) for virtual address encoding format.

**Module Sharing**: Same module bytecode used across multiple pages. Per-page targets would duplicate module registration, wasting IDs and creating address conflicts.

**LLDB Model Mismatch**: LLDB debugs processes with a single unified memory view, not isolated per-page address spaces. This is the fundamental architectural reason why virtual address fragmentation (above) becomes a problem - LLDB has no way to handle multiple disconnected address spaces within one process.

**Complex RWI Management**: Need to create/destroy targets as pages navigate, breaking active debug sessions when user navigates away from page being debugged.

**Coordination Overhead**: Breakpoints in shared modules would need cross-page synchronization, adding complexity and potential race conditions.

---

## Debugger Pause Model and Limitations

### Stop-the-World Design

WebAssembly debugging uses a **stop-the-world** pause model:
- When LLDB requests an interrupt or hits a breakpoint, ALL VMs in the process stop execution (main thread + all worker threads)

### Asymmetric Interaction with JavaScript Debugger

The Wasm debugger and JavaScript debugger have different pause mechanisms:

| Aspect | **Wasm Debugger (LLDB)** | **JavaScript Debugger (Web Inspector)** |
|--------|--------------------------|------------------------------------------|
| Pause model | Stop-the-world (blocks main thread) | Nested event loop (keeps processing events) |
| When paused | Main thread completely frozen | Event loop continues, JS execution suspended |
| IPC handling | WorkQueue processes messages | Main thread event loop processes messages |

### Known Limitation: Simultaneous Debugging

**Current Restriction**: Developers should use **either** the JavaScript debugger (Web Inspector) **or** the WebAssembly debugger (LLDB), but **not both simultaneously** on the same WebContent process.

**Why This Limitation Exists**:

When the Wasm debugger pauses at a breakpoint (stop-the-world), the main thread blocks completely. This causes:
1. **JavaScript execution freezes** - JS and Wasm share the same main thread
2. **Web Inspector becomes unresponsive** - Web Inspector's IPC runs on the frozen main thread (unlike Wasm debugger which uses WorkQueue)
3. **From the user's perspective** - Web Inspector appears frozen or disconnected

The interaction is asymmetric:
- **Pausing in Wasm debugger** → Blocks main thread → **Freezes both JS execution and Web Inspector**
- **Pausing in JS debugger** → Runs nested event loop → **Wasm debugger continues working** (uses WorkQueue IPC)

**Why Stop-the-World Is Correct**:

This design is intentional and matches standard native debugging:
- **Thread safety**: Prevents race conditions when inspecting Wasm memory/state
- **Consistent state**: Guarantees no concurrent modifications during inspection
- **LLDB expectations**: Matches how LLDB debugs native C++ code (entire process pauses)

**Current Scope**:

The current use case targets specialized debugging scenarios, where users are typically:
- Debugging JSC Wasm engine implementation
- Doing low-level WebAssembly performance analysis with LLDB
- **Not** general web developers debugging mixed JS/Wasm applications

**Future Considerations**:

If this becomes more widely used for general web development:
1. **Document clearly**: Explain that pausing Wasm freezes Web Inspector
2. **UI indicators**: Show warnings when both debuggers are active
3. **Consider alternatives**: Though stop-the-world is likely the only feasible model for LLDB integration

**Note**: Web Inspector's JavaScript debugger does not currently support WebAssembly debugging. When JavaScript calls WebAssembly functions, Web Inspector treats them as opaque function calls and cannot step into Wasm code or inspect Wasm state.
