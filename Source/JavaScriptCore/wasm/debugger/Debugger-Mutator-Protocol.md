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
m_mutatorContinue             // Debugger signals mutator to continue
m_debuggerContinue            // Mutator signals debugger to continue

m_debuggerState               // Current debugger operation state
m_vm                          // Target VM being debugged
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
Debugger Thread                         Mutator Thread (Target)                                    Other Thread(s)
==================       ==========================================================================================================================
                                      ExecutionHandler::stopCode()                            VMManager::notifyVMStop()
                                   ┌──>├─ wait(mutatorCV)                            ┌────────>├─ wait(worldCV)
resumeImpl()                       │   │                                             │         └─ Other VMs exiting notifyVMStop()                    
  ├─ Set state = ContinueRequested │   │                                             │ 
  ├─ notifyOne(mutator) ───────────┘   │                                             │ 
  ├─ wait(debuggerCV) <──┐             │                                             │ 
  │                      │             │                                             │ 
  │                      │             ├─ Check state -> ContinueRequested           │                
  │                      │             ├─ Set m_awaitingResumeNotification = true    │ 
  │                      │             └─ Return ResumeMode::All                     │ 
  │                      │                └─ VMManager::resumeAll() ─────────────────┘ 
  │                      │                    └─ Target VM exiting notifyVMStop()            
  │                      │                                                                Some VMs May exit notifyVMStop() early and hit breakpoints
  │                      │                                                                 ├─ stopTheWorld()
  │                      │                                                            ┌───>├─ wait(mutatorCV) on m_awaitingResumeNotification
  │                      │ Last VM exiting notifyVMStop() ─> wasmDebuggerOnResume()   │    │
  │                      │                                   ├─ handlePostResume()    │    │  
  │                      │                                   ├─ Set m_awaitingResumeNotification = false
  │                      └───────────────────────────────────├─ notifyOne(debuggerCV) │    │  
  │                                                          └─ notifyAll(mutator) ───┘    │ 
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
  │                      │               ├─                               └─ Other VMs wait(worldCV)
  │                      │               ├─ One VM picked as target
  │                      │               ├─ stopCode()      
  │                      │               ├─ setStopped()
  │                      │               ├─ Check state -> InterruptRequested
  │                      └───────────────┼─ notifyOne(debuggerCV)
  │                                      └─ wait(mutatorCV)
  │                      
  └─ sendStopReply()
```

**Key Points:**

- **Asynchronous**: Debugger initiates trap via VMManager
- All VMs stopped via trap mechanism (no direct wait/notify initially)
- Once target VM stops, it notifies debugger
- Multiple VMs coordinate through VMManager stop-the-world

---

### 3. **Single Step Operation**

```
Debugger Thread                         Mutator Thread (Target)                                    Other Thread(s)
==================       ==========================================================================================================================
                                      ExecutionHandler::stopCode()                            VMManager::notifyVMStop()
                                   ┌──>├─ wait(mutatorCV)                                      └─ wait(worldCV)
step()                             │   │                                        
  ├─ stepAtBreakpoint()            │   │                                    
  │   └─ Set one-time breakpoints  │   │                                    
  ├─ Set state = StepRequested     │   │                                    
  ├─ notifyOne(mutator) ───────────┘   │                                    
  ├─ wait(debuggerCV) <──┐             │                                    
  │                      │             ├─ Check state -> StepRequested      
  │                      │             └─ Return ResumeMode::One            
  │                      │                └─ VMManager::resumeOne(targetVM) 
  │                      │                    └─ Target VM exiting notifyVMStop() (others stay stopped)
  │                      │                        ├─ Target VM executes
  │                      │                        ├─ Hits one-time breakpoint
  │                      │                        └─ stopTheWorld()
  │                      │                            └─ notifyVMStop()
  │                      │                                └─ stopCode()
  │                      │                                    ├─ setStopped()
  │                      │                                    ├─ Check state -> StepRequested
  │                      └────────────────────────────────────┼─ notifyOne(debuggerCV)
  │                                                           └─ wait(mutatorCV)
  └─ sendStopReply()
```

**Key Points:**

- One-time breakpoints set before resume
- Only target VM runs (RunOne mode)
- All other VMs remain stopped
- Breakpoint hit triggers another stop-the-world

---

### 4. **Step-Into (Call/Throw)** - Two-Phase Protocol

```
Debugger Thread                         Mutator Thread (Target)                                    Other Thread(s)
==================       ==========================================================================================================================
                                      ExecutionHandler::stopCode()                            VMManager::notifyVMStop()
                                   ┌──>├─ wait(mutatorCV)                                      └─ wait(worldCV)
step()                             │   │                                          
  └─ stepAtBreakpoint()            │   │                                 
      ├─ Detect call/throw         │   │                                 
      ├─ Set hasStepIntoEvent flag │   │                                 
      ├─ notifyOne(mutator) ───────┘   │                                 
      ├─ wait(debuggerCV) <──┐         │                                 
      │                      │         ├─ Wakes up, executes call/throw  
      │                      │         └─ setStepIntoBreakpointForCall/Throw()             
      │                      │             ├─ Set breakpoint at callee or exception handler   
      │                      │             └─ stopTheWorld()           
      │                      │                 └─ notifyVMStop()
      │                      │                     ├─ stopCode(StepIntoSiteReached)
      │                      │                     ├─ Check state -> StepRequested                 
      │                      └─────────────────────┼─ notifyOne(debuggerCV)
      │                           ┌──────────────> ├─ wait(mutatorCV)
      │                           │                │
      │                           │                │
      ├─ notifyOne(mutator) ──────┘                │
      ├─ wait(debuggerCV) <┐                       │
      │                    │                       ├─ Wakes up, runs to step-into breakpoint
      │                    │                       ├─ Breakpoint hit -> stopCode()
      │                    │                       ├─ setStopped()
      │                    │                       ├─ Check state -> StepRequested
      │                    └───────────────────────┼─ notifyOne(debuggerCV)
      │                                            └─ wait(mutatorCV)
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
Debugger Thread                    Old Mutator Thread (Old Target)         New Mutator Thread (New Target)              Other Thread(s)
==================           ========================================    ==================================        ========================
                               ExecutionHandler::stopCode()               VMManager::notifyVMStop()                 VMManager::notifyVMStop()
                          ┌─────>├─ wait(mutatorCV)                     ┌──>├─ wait(worldCV)                         └─ wait(worldCV)
switchTarget(threadId)    │      │                                      │   │                                
  ├─ m_vm = newTargetVM   │      │                                      │   │
  ├─ Set state = SwitchRequested │                                      │   │
  ├─ notifyOne(mutator)───┘      │                                      │   │
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
  │                                                                                 └─ wait(mutatorCV)
  └── sendStopReply()
```

**Key Points:**

- Switch debugging target between VMs
- VMManager coordinates transition
