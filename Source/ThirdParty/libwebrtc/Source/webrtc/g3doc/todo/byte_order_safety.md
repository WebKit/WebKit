# Objective
Migrate the `rtc_base/byte_order.h` API from unsafe `void*` pointers to `webrtc::ArrayView<uint8_t>` to improve type safety and bounds checking. This migration follows a multi-phase approach to ensure backward compatibility for external consumers, provide clear deprecation warnings, and eventually remove the unsafe API.

# Key Files & Context
- `rtc_base/byte_order.h`: The target API for conversion.
- `api/array_view.h`: The replacement type for buffer spans.
- 19 identified files with internal usages of the legacy API.

# Implementation Steps

## Phase 0: Documentation
- Create `g3doc/todo/byte_order_safety.md` and store this migration plan there for tracking.

## Phase 1: Introduce New API
- Modify `rtc_base/byte_order.h` to include `#include "api/array_view.h"`.
- Implement `ArrayView` overloads for all `Get/Set` functions (`Get8`, `Set8`, `GetBE16`, etc.).
- Ensure new implementations include `RTC_DCHECK_GE(data.size(), ...)` for safety.
- **Note**: Do not add deprecation attributes yet.

## Phase 2: Migrate Internal Usages
- Replace all internal usages of the legacy `void*` API in the WebRTC codebase with the new `ArrayView` API.
- This ensures that WebRTC itself does not trigger deprecation warnings/errors once the attributes are added.

## Phase 3: Deprecation and External Notice
- Add `ABSL_DEPRECATE_AND_INLINE()` to the legacy `void*` overloads in `rtc_base/byte_order.h`.
- **External Action (Human)**: Send a deprecation notice to relevant mailing lists (e.g., `discuss-webrtc`) informing external consumers of the new `ArrayView` API and the impending removal of the `void*` API.
- Allow for a transition period for external projects to update their code.

## Phase 4: Final Deletion
- After the transition period, remove the deprecated `void*` functions and the associated deprecation attributes, leaving only the `ArrayView` API.

# Verification & Testing
- **Compilation**: Perform a full build (`autoninja -C out/Default`) to ensure no internal usages of deprecated functions remain.
- **Unit Tests**: Run `out/Default/rtc_unittests --gtest_filter=ByteOrderTest.*` and verify all tests pass with the new API.
- **Presubmit**: Run `git cl format` and `git cl presubmit -u --force`.
