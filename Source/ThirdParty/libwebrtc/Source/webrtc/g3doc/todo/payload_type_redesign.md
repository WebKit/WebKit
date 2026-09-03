# Payload type allocation redesign

The bug associated with this work is webrtc:360058654

## Background: What a payload type is

The payload type is a property of a codec that is established between a sender
and a receiver using SDP offer/answer.

Each end of the connection independently indicates the list of codecs it is
willing to send and/or receive, and assigns a payload type to each. It is
conventional but not required to use the same payload types in an answer as
those suggested in an offer.

For SDP sendonly media sections, it indicates a willingness to send; for
recvonly media sections, it indicates a willingness to receive; for sendrecv
media sections, it indicates a willingness to receive AND a willingness to send.
Presence in a media section does not guarantee that it will be used. (RFC 3264)

The `Codec` class represents the configuration of a particular payload type as
represented in an SDP offer/answer, which also controls how the data is
transferred on the wire. A better name would have been something like
`RtpPayloadTypeConfiguration`, but the `Codec` name is embedded quite widely in
the code.

## Current allocation strategy

The payload types are assigned by the VideoEngine and VoiceEngine according to a
fixed list. If the assignments collide with already established payload types,
the assignments are munged in the CodecVendor class so that there are no
colliding payload types.

The PayloadTypeRecorder class records what payload types are assigned - there is
one case (PR-answer followed by a later Answer that has different PTs) where
they are changed, but otherwise, once the PayloadTypeRecorder has assigned them,
that PT is permanent for that transport and that direction.

Picking a payload type is done in the PayloadTypePicker class. This is preloaded
with some assignments, so that if a PT is free, the commonly used PT for that
codec can be used.

```
          +-----------------------------------+
          | PeerConnection's PayloadTypePicker|
          +-----------------+-----------------+
                            |
              +-------------+-------------+
              |                           |
      +-------v-------+           +-------v-------+
      |  VoiceEngine  |           |  VideoEngine  |
      +-------+-------+           +-------+-------+
              |                           |
              |      (Initial PTs)        |
              v                           v
      +-------------------------------------------+    +-----------------------+
      |               CodecVendor                 |    |    Media section's    |
      |      (Munges PTs to avoid collisions)     +---->   PayloadTypePicker   |
      +-------+-------------+-------+-------------+    +-----------+-----------+
              |             |       |                              |
              |             |       |              +---------------v-------+
              |             |       +-------------->SdpPayloadTypeSuggester|
              |             |                      +---------------+-------+
              |             |                                      |
              v             v                                      v
      +----------------+ +------------------+<---------------------+
      |TypedCodecVendor| |PayloadTypeRecorder|
      +----------------+ +------------------+
```

## Desired future strategy

There should be no assignment of payload types in VideoEngine and VoiceEngine.
Payload types should be picked only when creating an offer or answer; they are
assigned permanently in SetLocalDescription / SetRemoteDescription.

Once an assignment is made, it should never need to change (apart from the case
noted above).

```
          +-------------+           +-------------+
          | VoiceEngine |           | VideoEngine |
          +------+------+           +------+------+
                 |                         |
                 |  (CodecConfigurations)  |
                 |    (No initial PTs)     |
                 v                         v
          +-------------------------------------------+    +-----------------------+
          |               CodecVendor                 |    |    Media section's    |
          |   (Assigns PTs and expands resiliency)    +---->   PayloadTypePicker   |
          +-------+-----------------------------------+    +-----------+-----------+
                  |                                                    |
                  v                                        +-----------v-----------+
          +----------------+                               |SdpPayloadTypeSuggester|
          |TypedCodecVendor|                               +-----------+-----------+
          +----------------+                                           |
                                                           +-----------v-----------+
                                                           | PayloadTypeRecorder & |
                                                           | PeerConnection's PT   |
                                                           | Picker                |
                                                           +-----------------------+
```

## Dealing with resiliency mechanisms

Resiliency mechanisms (RED, RTX) are signaled using "codecs", and are
traditionally represented as such within the library. They associate with the
codec they are resiliency for by using the "apt=" parameter in their a=fmtp
lines. However, this complicates assignment, since that parameter cannot be
filled in until the payload type for the "protected" codec is assigned.

## Backwards compatibility issues

Experience has shown that changing the PT allocation strategy often trips up
applications that have depended on the result of the old allocation strategy, so
user-visible changes should be avoided. One example is that it's preferred (but
not strictly required) to have the RTX PT assigned the PT number of 1 more than
the PT of the codec it refers to.

## Current implementation status

The redesign is now largely implemented for both audio and video codecs when the
`WebRTC-PayloadTypesInTransport` field trial is enabled. Key milestones reached:

-   **Bifurcated Negotiation Logic:** `CodecVendor` now has separate paths for
    legacy and redesigned PT allocation. The redesigned path uses
    `CodecConfiguration` and `MergeCodecsFromConfigurations` for all media
    types.
-   **Unified Resiliency Expansion:** Late expansion of RTX, RED, ULPFEC, and
    FlexFEC is handled uniformly in `pc/codec_vendor.cc`.
-   **Audio/Video RED Collision:** Fixed by enforcing media type equality in
    matching rules.
-   **MID Recycling:** Correctly handled with media type validation, preventing
    invalid codec merging when MIDs are reused.
-   **Stable PT Assignment:** Verified to maintain payload type stability across
    renegotiations and codec preference changes.
-   **Conventional RTX Assignment:** RTX PTs now default to `Primary_PT + 1` to
    maintain backwards compatibility with legacy expectations.

The implementation is verified by a dedicated suite of integration tests in
`pc/codec_vendor_redesign_unittest.cc`.

## Unified Implementation Strategy for Audio and Video

The transition to a unified late-assignment model is nearly complete, using
internal `CodecConfiguration` objects to represent codecs before they are
assigned payload types.

### 1. CodecConfiguration and ResiliencyInfo

Introduced in `pc/codec_configuration.h`:

-   **`ResiliencyInfo`**: Encapsulates the redundancy requirements (RTX, RED,
    ULPFEC, FlexFEC).
-   **`CodecConfiguration`**: Stores codec attributes and their associated
    `ResiliencyInfo`. This allows the engine to express capabilities without
    pre-assigning payload types.

### 2. Unified Codec Collection

`TypedCodecVendor` handles the bifurcated collection path:

-   **Redesigned Path**: Collects `CodecConfiguration` objects from the media
    engine factories. It also performs a "legacy expansion" to populate the
    internal `codecs()` list for compatibility with existing code that expects
    pre-assigned PTs.
-   **Legacy Path**: Continues to use the engine's `LegacySendCodecs` /
    `LegacyRecvCodecs` methods.

### 3. Late Expansion and Parameter Linking

`CodecVendor::MergeCodecsFromConfigurations` performs the following for all
media types:

1.  Assigns a payload type to the primary media codec via
    `SdpPayloadTypeSuggester`.
2.  Expands the `ResiliencyInfo` into redundancy `Codec` objects (RTX, RED,
    FEC).
3.  Links redundancy codecs to the primary PT (e.g., setting the `apt` parameter
    for RTX).

**Current Status:** RTX and RED linking are fully unified. RED linking for audio
has been refactored into the unified expansion logic in `MergeRedCodec`.

## Testing Strategy

To ensure correctness and prevent regressions while the
`WebRTC-PayloadTypesInTransport` field trial is being developed, a "Redesign
Feedback Loop" strategy is used:

1.  **Identify failing tests:** Run full suites (`rtc_unittests`,
    `peerconnection_unittests`) with the trial enabled.
2.  **Reproduction and Isolation:** Failing cases are ported to
    `pc/codec_vendor_redesign_unittest.cc` for focused debugging.
3.  **Surgical Fixes:** Fixes are verified against the isolated tests and then
    re-verified against the full suite.
4.  **Full Re-verification:** Once the tests are stable, run all tests without
    the field trial flag to ensure there are no regressions, and then either ask
    to commit this set of changes or loop back to step 1.

### Canary Branch Strategy

To ensure that no unit tests are missed, a "canary branch" approach is used.

1.  **Canary Branch (`pt-enable`)**: Maintain a branch where the field trial is
    forced enabled by default. This branch is used to run the full WebRTC test
    suite (especially `rtc_pc_unittests` and `peerconnection_unittests`) to
    identify all edge cases and legacy behaviors that the redesign logic doesn't
    yet handle.
2.  **Reproduction and Isolation**: When a failure is identified on the canary
    branch, the specific test case is cloned or ported into a specialized
    integration test file (`pc/codec_vendor_redesign_unittest.cc`) on the
    implementation branch. This allows for focused debugging and ensures the
    failure is reproducible in a clean environment with the trial explicitly
    enabled.
3.  **Surgical Fixes**: Fixes are developed and verified on the implementation
    branch using the isolated tests.
4.  **Full Re-verification**: Once the implementation branch is stable, the
    canary branch is rebased to include the fixes, and the full test suite is
    run again to ensure no remaining failures and to catch new regressions.

## Backwards Compatibility for Unit Testing

Test helpers like `CodecLookupHelperForTesting` are used in legacy unit tests to
"pre-seed" the `FakePayloadTypeSuggester` with hardcoded PT expectations. This
allows tests that depend on specific PT values to pass while the underlying
allocation logic transitions to a more generic, transport-aware strategy.
