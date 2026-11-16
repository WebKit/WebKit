# WebAssembly Debugger Test Framework

A simplified, high-performance testing framework for WebAssembly debugging functionality in WebKit's JavaScriptCore with intelligent parallel execution.

## 🏗️ Architecture

### Sequential Mode (Default)

**Single Test Execution**: One test at a time using port 12340

```txt
Framework Process (PID: 12345):
├── Main Thread (TID: 100) - coordinates execution
└── Per Test:
    ├── JSC Process (PID: 12346)
    ├── LLDB Process (PID: 12347)
    └── 4 Monitor Threads (TIDs: 101-104): JSC stdout/stderr, LLDB stdout/stderr

Total: 3 Processes, 5 Threads per test
Simple, reliable execution for debugging individual tests
```

### Parallel Mode (`--parallel`)

**Multi-Worker Execution**: ThreadPoolExecutor with smart worker calculation

- Each worker = thread that runs one test case
- Each worker gets unique port and ProcessManager
- I/O-bound optimization: More workers than CPU cores for better utilization

**Example: 3 Tests, 2 Workers**

```txt
Framework Process (PID: 12345):
├── Main Thread (TID: 100) - coordinates execution
├── Worker Thread 1 (TID: 101) - TestA (Port: 12340)
│   ├── JSC Process (PID: 12346)
│   ├── LLDB Process (PID: 12347)
│   └── 4 Monitor Threads (TIDs: 103-106): JSC stdout/stderr, LLDB stdout/stderr
└── Worker Thread 2 (TID: 102) - TestB (Port: 12341)
    ├── JSC Process (PID: 12348)
    ├── LLDB Process (PID: 12349)
    └── 4 Monitor Threads (TIDs: 107-110): JSC stdout/stderr, LLDB stdout/stderr

Peak Concurrent: 5 Processes, 11 Threads
Total Used: 7 Processes, 15 Threads (when TestA finishes, Worker 1 picks up TestC)
```

### Framework Structure

```txt
lib/
├── core/                   # Core framework components
│   ├── base.py             # BaseTestCase with pattern matching
│   ├── environment.py      # WebKit environment setup
│   ├── registry.py         # Test case registry
│   └── utils.py            # Logging with PID/TID support
├── discovery/              # Test discovery system
│   └── auto_discovery.py   # Automatic test case discovery
└── runners/                # Test execution engines
    ├── sequential.py       # Sequential test runner
    ├── parallel.py         # Parallel test execution
    └── process_manager.py  # Per-test process isolation
tests/                      # Auto-discovered test cases
├── add.py                  # WebAssembly debugging tests
└── ...                     # Additional test cases
```

## 🧪 Writing Test Cases

Create file in `tests/my_test.py`

```python
from lib.core.base import BaseTestCase

class MyTestCase(BaseTestCase):
    def execute(self):
        # Setup debugging session (JSC + LLDB)
        self.setup_debugging_session_or_raise("resources/c-wasm/add/main.js")
        
        # Send LLDB commands
        self.send_lldb_command_or_raise("b main")
        self.send_lldb_command_or_raise("c")
        
        # Validate results
        self.send_lldb_command_or_raise("dis")
```

**Auto-Discovery**: Test automatically appears in `--list` and runs with framework. No manual registration required.

## 📋 Requirements

### LLDB Configuration

The test framework requires a **TOT (Tip of Tree) built LLDB** with WebAssembly debugging support, not the system LLDB. The LLDB path is hardcoded in [`lib/core/environment.py`](lib/core/environment.py):

```python
hardcoded_lldb = "/Users/yijiahuang/git/WebKit/llvm/build/bin/lldb"
```

**Why TOT LLDB is Required:**
- System LLDB lacks the latest WebAssembly debugging features
- TOT LLDB includes WebAssembly plugin support (`--plugin wasm`)
- Custom LLDB build ensures compatibility with WebKit's WASM debugging protocol

**Fallback Behavior:**
- If the hardcoded path doesn't exist, the framework falls back to system `lldb`
- Tests may fail with system LLDB due to missing WebAssembly debugging capabilities

To build the required LLDB, follow the WebKit LLVM build instructions.

## 🚀 Quick Start

```bash
# Run all tests (sequential)
python3 test-wasm-debugger.py

# Run all tests in parallel (auto-detects optimal workers)
python3 test-wasm-debugger.py --parallel

# Run with specific number of parallel workers
python3 test-wasm-debugger.py --parallel=4

# Run specific test with verbose logging
python3 test-wasm-debugger.py --test AddTestCase --verbose

# List all available tests
python3 test-wasm-debugger.py --list

# Use specific build configuration
python3 test-wasm-debugger.py --debug    # Force Debug build
python3 test-wasm-debugger.py --release  # Force Release build
```
