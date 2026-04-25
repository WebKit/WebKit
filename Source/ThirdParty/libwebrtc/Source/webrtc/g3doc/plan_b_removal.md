<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2026-01-17'} *-->

# Plan B Deprecation and Removal

This document describes the tooling and process for deprecating and eventually removing Plan B SDP semantics from the WebRTC codebase.

## Build Configuration

To support a gradual deprecation, a new GN argument and C++ macro have been introduced.

### GN Argument: `deprecate_plan_b`

Defined in `webrtc.gni`, this argument controls whether Plan B specific APIs are marked as deprecated at compile time.

*   **`false` (default):** Plan B APIs are available without warnings.
*   **`true`:** Plan B APIs are annotated with `[[deprecated]]`. In WebRTC's default build configuration (which treats warnings as errors), this will cause compilation to fail if any Plan B APIs are used.

To enable this in your local build, add the following to your `args.gn`:
```gn
deprecate_plan_b = true
```

### C++ Macro: `PLAN_B_ONLY`

Defined in `rtc_base/system/plan_b_only.h`, this macro should be used to annotate functions, classes, or variables that are only needed for Plan B semantics.

```cpp
#include "rtc_base/system/plan_b_only.h"

PLAN_B_ONLY void MyPlanBSpecificFunction();
```

- When `deprecate_plan_b` is `true`, `PLAN_B_ONLY` expands to `[[deprecated]]`.
- When `deprecate_plan_b` is `false`, `PLAN_B_ONLY` expands to an empty string.

### Suppression Macros

To allow existing Plan B code to compile when `deprecate_plan_b = true`, the following macros are provided in `rtc_base/system/plan_b_only.h`:

- `RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN()`
- `RTC_ALLOW_PLAN_B_DEPRECATION_END()`

These macros use compiler pragmas to locally disable deprecation warnings.

## Usage Guidelines

### Annotating New Plan B Code
Avoid adding new Plan B code. If it is absolutely necessary, ensure it is annotated with the `PLAN_B_ONLY` macro.

### Identifying Plan B Dependencies
By setting `deprecate_plan_b = true`, developers can identify all call sites that still rely on Plan B. This is the primary method for auditing the progress of the migration to Unified Plan.

### Suppressing Deprecation Warnings in Legacy Code
Existing Plan B call sites in "glue code" (e.g., `PeerConnection` delegation logic) should be wrapped with the suppression macros to allow the build to pass.

```cpp
RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN();
sdp_handler_->RemoveStream(local_stream);
RTC_ALLOW_PLAN_B_DEPRECATION_END();
```

This ensures that the build only fails on *new*, unwrapped Plan B usage, while clearly marking legacy code that requires migration.

### Transitioning to Unified Plan
Functions that are compatible with both Plan B and Unified Plan should **not** be annotated. Only those that are functionally incorrect or redundant under Unified Plan (often guarded by `RTC_DCHECK(!IsUnifiedPlan())`) should be marked.
