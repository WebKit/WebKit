# Debugger-Mutator Communication Protocol

**ExecutionHandler Thread Synchronization in WebAssembly Debugging**

This document describes the thread communication protocol used in `WasmExecutionHandler.cpp` to coordinate between the debugger thread (LLDB commands) and mutator threads (WebAssembly execution), along with VMManager coordination for stop-the-world operations.

---

## Thread Actors

### 1. **Debugger Thread**

- Runs on the debug server's WorkQueue thread
- Processes LLDB commands: continue, step, interrupt, thread selection
- Initiates debugging operations
- Thread ID: `m_debugServerThreadId`

### 2. **Mutator Thread(s)**

- Runs WebAssembly bytecode execution
- One per VM (main thread + worker threads)
- Responds to debugging commands
- Thread ID: `VM::ownerThread()->uid()`

### 3. **VMManager** (Coordinator)

- Singleton managing all VMs in the process
- Implements stop-the-world coordination
- Calls `wasmDebuggerOnStop()` and `wasmDebuggerOnResume()` callbacks
- Ensures all VMs stopped before debugger accesses state

---

## Synchronization Primitives

### Key State Variables

```cpp
m_debuggeeContinue            // Debugger signals debuggee to continue
m_debuggerContinue            // Debuggee signals debugger to continue

m_debuggerState               // Current debugger operation state
m_debuggee                    // VM being debugged
m_awaitingResumeNotification  // Resume barrier flag
```

### Debugger States

```cpp
enum class DebuggerState {
    Replied,              // Idle, ready for next command
    InterruptRequested,   // Async interrupt in progress
    ContinueRequested,    // Resume all VMs
    StepRequested,        // Single-step with breakpoints
    SwitchRequested       // VM context switch
};
```

---

## Protocol Flow Diagrams

### 1. **Continue Operation** (Resume All VMs)

```
Debugger Thread                             Debuggee Thread                                           Other Thread(s)
==================       ==========================================================================================================================
                                      ExecutionHandler::stopCode()                            VMManager::notifyVMStop()
                                   ┌──>├─ wait(debuggeeCV)                           ┌────────>├─ wait(worldCV)
resumeImpl()                       │   │                                             │         └─ Other VMs exiting notifyVMStop()
  ├─ Set state = ContinueRequested │   │                                             │
  ├─ notifyOne(debuggeeCV) ────────┘   │                                             │
  ├─ wait(debuggerCV) <──┐             │                                             │
  │                      │             │                                             │
  │                      │             ├─ Check state -> ContinueRequested           │
  │                      │             ├─ Set m_awaitingResumeNotification = true    │
  │                      │             └─ Return ResumeMode::All                     │
  │                      │                └─ VMManager::resumeAll() ─────────────────┘
  │                      │                    └─ Debuggee VM exiting notifyVMStop()
  │                      │                                                                Some VMs May exit notifyVMStop() early and hit breakpoints
  │                      │                                                                 ├─ stopTheWorld()
  │                      │                                                            ┌───>├─ wait(debuggeeCV) on m_awaitingResumeNotification
  │                      │ Last VM exiting notifyVMStop() ─> wasmDebuggerOnResume()   │    │
  │                      │                                   ├─ handlePostResume()    │    │  
  │                      │                                   ├─ Set m_awaitingResumeNotification = false
  │                      └───────────────────────────────────├─ notifyOne(debuggerCV) │    │  
  │                                                          └─ notifyAll(debuggeeCV) ┘    │ 
  └─ Debugger resumes                                                                      └─ notifyVMStop() and send stop reply to debugger
```

**Key Points:**

- Debugger waits for **post-resume** confirmation to prevent interrupt() race
- `m_awaitingResumeNotification` acts as resume barrier
- VMManager ensures all VMs actually resumed before callback

---

### 2. **Interrupt Operation** (Async Stop Request)

```
Debugger Thread                                               All Thread(s)
==================         ===========================================================================================
                                                            [Running Wasm]
interrupt()
  ├─ Set state = InterruptRequested
  ├─ VMManager::requestStopAll() ──────────────────-----───> VMTraps fire
  ├─ wait(debuggerCV) <──┐              notifyVMStop()                    notifyVMStop()
  │                      │               ├─ One VM picked as debuggee      └─ Other VMs wait(worldCV)
  │                      │               ├─ On stop callback
  │                      │               ├─ stopCode()
  │                      │               ├─ setStopped()
  │                      │               ├─ Check state -> InterruptRequested
  │                      └───────────────┼─ notifyOne(debuggerCV)
  │                                      └─ wait(debuggeeCV)
  │
  └─ sendStopReply()
```

**Key Points:**

- **Asynchronous**: Debugger initiates trap via VMManager
- All VMs stopped via trap mechanism (no direct wait/notify initially)
- Once debuggee VM stops, it notifies debugger
- Multiple VMs coordinate through VMManager stop-the-world

---

### 3. **Single Step Operation**

```
Debugger Thread                         Mutator Thread (Debuggee)                                  Other Thread(s)
==================       ==========================================================================================================================
                                      ExecutionHandler::stopCode()                            VMManager::notifyVMStop()
                                   ┌──>├─ wait(debuggeeCV)                                      └─ wait(worldCV)
step()                             │   │
  ├─ stepAtBreakpoint()            │   │
  │   └─ Set one-time breakpoints  │   │
  ├─ Set state = StepRequested     │   │
  ├─ notifyOne(debuggeeCV) ────────┘   │
  ├─ wait(debuggerCV) <──┐             │
  │                      │             ├─ Check state -> StepRequested
  │                      │             └─ Return ResumeMode::One
  │                      │                └─ VMManager::resumeOne(debuggee)
  │                      │                    └─ Debuggee VM exiting notifyVMStop() (others stay stopped)
  │                      │                        ├─ Debuggee VM executes
  │                      │                        ├─ Hits one-time breakpoint
  │                      │                        └─ stopTheWorld()
  │                      │                            └─ notifyVMStop()
  │                      │                                └─ stopCode()
  │                      │                                    ├─ setStopped()
  │                      │                                    ├─ Check state -> StepRequested
  │                      └────────────────────────────────────┼─ notifyOne(debuggerCV)
  │                                                           └─ wait(debuggeeCV)
  └─ sendStopReply()
```

**Key Points:**

- One-time breakpoints set before resume
- Only debuggee VM runs (RunOne mode)
- All other VMs remain stopped
- Breakpoint hit triggers another stop-the-world

---

### 4. **Step-Into (Call/Throw)** - Two-Phase Protocol

```
Debugger Thread                         Mutator Thread (Debuggee)                                  Other Thread(s)
==================       ==========================================================================================================================
                                      ExecutionHandler::stopCode()                            VMManager::notifyVMStop()
                                   ┌──>├─ wait(debuggeeCV)                                      └─ wait(worldCV)
step()                             │   │
  └─ stepAtBreakpoint()            │   │
      ├─ Detect call/throw         │   │
      ├─ Set hasStepIntoEvent flag │   │
      ├─ notifyOne(debuggeeCV) ────┘   │
      ├─ wait(debuggerCV) <──┐         │                                 
      │                      │         ├─ Wakes up, executes call/throw  
      │                      │         └─ setStepIntoBreakpointForCall/Throw()             
      │                      │             ├─ Set breakpoint at callee or exception handler   
      │                      │             └─ stopTheWorld()           
      │                      │                 └─ notifyVMStop()
      │                      │                     ├─ stopCode(StepIntoSiteReached)
      │                      │                     ├─ Check state -> StepRequested                 
      │                      └─────────────────────┼─ notifyOne(debuggerCV)
      │                           ┌──────────────> ├─ wait(debuggeeCV)
      │                           │                │
      │                           │                │
      ├─ notifyOne(debuggeeCV) ───┘                │
      ├─ wait(debuggerCV) <┐                       │
      │                    │                       ├─ Wakes up, runs to step-into breakpoint
      │                    │                       ├─ Breakpoint hit -> stopCode()
      │                    │                       ├─ setStopped()
      │                    │                       ├─ Check state -> StepRequested
      │                    └───────────────────────┼─ notifyOne(debuggerCV)
      │                                            └─ wait(debuggeeCV)
      └─ sendStopReply()
```

**Key Points:**

- **Two-phase handshake**:
  1. Execute call/throw and register breakpoint
  2. Resume and hit step-into breakpoint
- Breakpoint set dynamically at call/throw site

---

### 5. **VM Context Switch**

```
Debugger Thread                    Old Mutator Thread (Old Debuggee)       New Mutator Thread (New Debuggee)            Other Thread(s)
==================           ========================================    ==================================        ========================
                               ExecutionHandler::stopCode()               VMManager::notifyVMStop()                 VMManager::notifyVMStop()
                          ┌─────>├─ wait(debuggeeCV)                    ┌──>├─ wait(worldCV)                         └─ wait(worldCV)
switchTarget(threadId)    │      │                                      │   │
  ├─ m_debuggee = newDebuggee    │                                      │   │
  ├─ Set state = SwitchRequested │                                      │   │
  ├─ notifyOne(debuggeeCV)┘      │                                      │   │
  ├─ wait(debuggerCV) <────┐     │                                      │   │
  │                        │     ├─ Check state -> SwitchRequested      │   │
  │                        │     └─ Return ResumeMode::Switch           │   │
  │                        │         └─ VMManager switch context ───────┘   │
  │                        │             └─ wait(worldCV)                   │
  │                        │                                                └─ wasmDebuggerOnStop()
  │                        │                                                    └─ handleStopTheWorld() -> stopCode()
  │                        │                                                        ├─ setStopped()
  │                        │                                                        ├─ Check state -> SwitchRequested
  │                        └────────────────────────────────────────────────────────┼─ notifyOne(debuggerCV)
  │                                                                                 └─ wait(debuggeeCV)
  └── sendStopReply()
```

**Key Points:**

- Switch debuggee between VMs
- VMManager coordinates transition
