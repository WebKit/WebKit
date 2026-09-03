<!-- go/cmark -->
# Plan: Restrict RtpTransceiver sender/receiver access to PLAN_B_ONLY

## Objective
Restrict access to `RtpTransceiver::senders_` and `RtpTransceiver::receivers_` such that access to all elements except the first one is only possible via functions marked `PLAN_B_ONLY`. This aligns the implementation with Unified Plan requirements where only one sender and one receiver are used, while preserving legacy Plan B functionality.

## Proposed Changes

### 1. Header Refactoring (`pc/rtp_transceiver.h`)
*   **Deprecate Vector Accessors**: Mark `senders()` and `receivers()` with the `PLAN_B_ONLY` macro. This triggers deprecation warnings for any code accessing the full lists, making Plan B dependencies explicit.
*   **Add Unified Plan Accessors**:
    *   Add `internal_first_sender()` and `internal_first_receiver()` private helpers.
    *   These helpers will return the first element (or `nullptr`) and will be used by Unified Plan paths.

### 2. Implementation Update (`pc/rtp_transceiver.cc`)
*   **Shared Logic Branching**: Methods that currently iterate over `senders_` or `receivers_` (e.g., `SetMediaChannels`, `GetStopSendingAndReceiving`, `GetDeleteChannelWorkerTask`, `OnNegotiationUpdate`) must be updated:
    *   **Unified Plan**: Use `internal_first_...` helpers to process only the primary element.
    *   **Plan B**: Wrap iterations in `RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN` and `RTC_ALLOW_PLAN_B_DEPRECATION_END`.
*   **Protect Index Access**: Ensure `senders_[0]` or `receivers_[0]` are only used in Unified Plan paths (guarded by `RTC_DCHECK(unified_plan_)`).

### 3. External Caller Migration
*   Update components that iterate over senders/receivers:
    *   `pc/legacy_stats_collector.cc`
    *   `pc/peer_connection.cc`
    *   `pc/rtc_stats_collector.cc`
    *   `pc/rtp_transmission_manager.cc`
*   Wrap these iterations in `RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN/END` to acknowledge the legacy dependency.

## Verification Plan
*   **Compilation**: Ensure all targets compile with and without `WEBRTC_DEPRECATE_PLAN_B` defined.
*   **Unit Tests**: Run `pc_unittests` and `peerconnection_unittests` to ensure no regression in both Unified Plan and Plan B modes.
