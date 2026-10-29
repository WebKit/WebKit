# Dangling Pointer Detector

A pointer is dangling when it references freed memory. Dangling pointers are
a source of Use-After-Free (UAF) bugs and are highly discouraged unless you can
definitively ensure that they will never be dereferenced or used after the
pointed-to object is freed.

ANGLE tests (run via `TestSuite`) are configured to detect dangling
`raw_ptr<T>` instances when built with PartitionAlloc enabled.

## Motivation

Tracking the lifetime of interacting objects across a complex C++ codebase is
difficult. Often, lifetime issues are discovered late when they cause
hard-to-reproduce user crashes.

Enforcing the Dangling Pointer Detector on the Commit Queue (CQ) helps us:
1. **Prevent Regressions**: Catch accidental lifetime changes that invalidate
   prior ownership assumptions before they ship.
2. **Promote Better Architecture**: Flag ambiguous object lifetimes during code
   review.
3. **Verify Cleanups**: Give developers immediate confirmation when cleaning
   up dangling pointers.

## `raw_ptr<T>`

A `raw_ptr<T>` is a non-owning smart pointer. When using `raw_ptr<T>`, the
severity of UAFs is significantly mitigated because the underlying allocation
is protected by [MiraclePtr / BackupRefPtr](https://security.googleblog.com/2022/09/use-after-freedom-miracleptr.html).

A `raw_ptr<T>` works transparently like a raw `T*`. It should primarily be used
for class and struct member variables.

## Flavors & Annotations

When a pointer must temporarily dangle safely, or represents an untriaged legacy
instance, you can use these annotations:

```cpp
raw_ptr<T> ptr_never_dangling;
raw_ptr<T, DisableDanglingPtrDetection> ptr_allowed_to_dangle;
raw_ptr<T, DanglingUntriaged> ptr_dangling_to_investigate;
```

* `DisableDanglingPtrDetection`: Used to annotate intentional and safe dangling
  pointers as a last resort if re-architecting ownership is impractical.
* `DanglingUntriaged`: Indicates a pre-existing dangling pointer marked for
  future cleanup.

## Diagnosing Dangling Pointers

When a dangling pointer release is detected in ANGLE standalone builds,
the test runner aborts with a crash backtrace at the point where the
`raw_ptr` was released (reassigned, cleared, or destructed).

The callbacks are registered in [`src/tests/test_utils/runner/PartitionAllocSupport.cpp`](../src/tests/test_utils/runner/PartitionAllocSupport.cpp):
1. `SetDanglingRawPtrDetectedFn`: Invoked when memory is freed while a
   `raw_ptr` still references it.
2. `SetDanglingRawPtrReleasedFn`: Invoked when the dangling `raw_ptr` is released,
   aborting the process with `FATAL()`.

To find where the pointed-to memory was **freed**:
Run the test under a debugger (`gdb` or `lldb`) and place a breakpoint inside
`SetDanglingRawPtrDetectedFn` in `PartitionAllocSupport.cpp`. The call stack
at that breakpoint reveals the exact code path freeing the memory.

*(Note: Chromium tests exercising ANGLE use Chrome's test runner, which
automatically records and displays both the memory-free stacktrace and the
`raw_ptr` release stacktrace.)*

## Enabling PartitionAlloc in Standalone Builds

PartitionAlloc is enabled by default on ANGLE CQ bots for assert builds.

To enable it locally in standalone ANGLE builds:
1. Add `"checkout_angle_partition_alloc": True` to `custom_vars` in your `.gclient` file:
   ```python
   solutions = [
     {
       "name": ".",
       "url": "https://chromium.googlesource.com/angle/angle.git",
       "custom_vars": {
         "checkout_angle_partition_alloc": True,
       },
     },
   ]
   ```
2. Run `gclient sync` to fetch PartitionAlloc into `third_party/partition_alloc`.
3. In assert-enabled builds (`dcheck_always_on = true` or `is_debug = true`), PartitionAlloc is enabled automatically. You can also explicitly set `angle_use_partition_alloc = true` in `args.gn`.

