# Todo: Outlaw Local SDP Munging

This document outlines the plan and remaining tasks for deprecating and
eventually removing support for local SDP munging in WebRTC/Chrome.

## Motivation

Local SDP munging (modifying the SDP session description between
`createOffer`/`createAnswer` and `setLocalDescription`) is a violation of WebRTC
standards
[WebRTC](https://w3c.github.io/webrtc-pc/#dom-peerconnection-setlocaldescription),
[JSEP](https://datatracker.ietf.org/doc/html/rfc8829#section-5.4). While
historically permitted in Chrome, it introduces significant security risks
(e.g., [localmess](https://localmess.github.io/)), compatibility issues (no
other major browser supports it), and architectural complexity.

The goal is to reach a state of **zero local SDP munging** by rejecting all
modifications.

- **Overarching Bug:** [webrtc:414284082](https://issues.webrtc.org/414284082)
- **Chromium Feature:**
  [SDP Munging Deprecation](https://chromestatus.com/feature/5166153227567104)
  (Tracking Issue: [crbug/40567530](https://issues.chromium.org/40567530))

The spec goal of "strings should compare equal" is hard to achieve, for a couple
of reasons:

- Parsing and regenerating the strings will cause some churn due to reordering,
  even if no real change has taken place
- Some features, like RTCP-FB with wildcards, will cause the regenerated SDP
  description to be different from the original due to distributing an
  overarching configuration down to the components of the description, or vice
  versa.

Therefore, the checker is designed to catch **semantic** changes, not
necessarily all **syntactic** changes.

## Usage monitoring

The usage of the various types of munge is tracked in WebRTC by the UMA
histograms named `WebRTC.PeerConnection.SdpMunging`. There are separate
histograms for Offer, Answer and PrAnswer, and each histogram tracks all the
forms of munge as selectable buckets. Munges are checked in sequence, so if an
instance applies multiple munges, only the one that is checked for first is
detected. Unlcassified munges are in the "UnknownModification" bucket; it is
good to update the detector to classify these munges as a new category whenever
they can be identified.

There are also munges that are more easily detected at the Chrome level and
counted in the `Blink.UseCounter.Features` UMA; these are buckets prefixed with
"RTCLocalSdpModification".

## Rollout Strategy

The deprecation uses an iterative rollout using the field trial
`WebRTC-NoSdpMangleReject`. This trial takes a comma-separated list of numeric
`SdpMungingType` values to disallow.

The rollout proceeds in phases:

1. **Block unused/rare munges:** Munges with low usage (\<0.01% in UMA) or clear
   security risks are blocked first.
2. **Provide APIs for valid use cases:** Legitimate use cases currently relying
   on munging must have spec-compliant APIs shipped before their corresponding
   munges can be blocked.
3. **Deprecation warnings:** Console warnings will be emitted in Chrome when
   munging is detected (targeting when "No modification" usage is high, e.g.
   \>90%).
4. **Full enforcement:** Once a blocked type reaches 100% stable rollout, it
   will be unconditionally rejected in code.

## Current Roadmap & Tasks

### Phase 1: Block Unused and Rare Munges (In Progress)

The following munging types have been identified as having near-zero usage
(\<0.01% in UMA) and are candidates for immediate blocking:

- **ICE Mode** (`kIceMode` = 23) - *Zero usage*
- **Audio Added L16** (`kAudioCodecsAddedL16` = 64)
- **Audio FMTP Opus FEC** (`kAudioCodecsFmtpOpusFec` = 66)
- **SSRCs** (`kSsrcs` = 27)
- **Number of Contents** (`kNumberOfContents` = 4)
- **Audio FMTP Opus CBR** (`kAudioCodecsFmtpOpusCbr` = 67)
- **ICE Pwd** (`kIcePwd` = 21)
- **Audio RTCP Reduced Size** (`kAudioCodecsRtcpReducedSize` = 73)
- **DTLS Setup** (`kDtlsSetup` = 24)
- **Video Codecs Modified with Raw Packetization**
  (`kVideoCodecsModifiedWithRawPacketization` = 88)
- **Without Create Answer** (`kWithoutCreateAnswer` = 2)

#### TODOs:

- [ ] Add these types to the default `WebRTC-NoSdpMangleReject` blocklist via
  Finch.
- [ ] Transition these blocked types to unconditional rejection in
  [sdp_munging_detector.cc](https://cs.chromium.org/chromium/src/third_party/webrtc/pc/sdp_munging_detector.cc?drc=d57bda1bb6222d9606f683fdfc50f0fbe0033f8b&l=763)
  once stable.

### Phase 2: Address High-Usage Munges (Legitimate Use Cases)

Several munging types are widely used because spec-compliant alternative APIs
are either not fully adopted or not yet implemented.

#### 1. Audio Codecs Opus Stereo (`kAudioCodecsFmtpOpusStereo` = 68)

- **Use case:** Enabling stereo playout for Opus.
- **Status:** Ongoing work to redefine Opus handling to enable stereo by
  default.
- **Tracking Bug:** [webrtc:41480879](https://issues.webrtc.org/41480879)
- **TODO:** Complete the Opus stereo default enablement and transition users.

#### 2. Raw Video Packetization (`kVideoCodecsAddedWithRawPacketization` = 87, `kVideoCodecsModifiedWithRawPacketization` = 88)

- **Use case:** Selecting raw packetization format for video.
- **Status:** Currently cannot select a new codec with specific packetization
  via `RTCRtpSender.setParameters`.
- **Tracking Bug:** [webrtc:42222566](https://issues.webrtc.org/42222566)
- **TODO:** Design and implement API for video packetization format control.

#### 3. RTCP XR Receiver RTT (`kAudioCodecsRtcpFbRrtr` = 72)

- **Use case:** Negotiating RTCP XR receiver RTT (`a=rtcp-fb:<pt> rrtr`).
- **Tracking Bug:** [webrtc:42222605](https://issues.webrtc.org/42222605)
- **TODO:** Add "rtcp-xt" SDP attributes negotiation support.

#### 4. RTP Header Extensions (`kRtpHeaderExtensionAdded` = 41, `kRtpHeaderExtensionRemoved` = 40, `kRtpHeaderExtensionModified` = 42)

- **Use case:** Adding, removing, or modifying RTP header extensions.
- **Tracking Bugs:** [chromium:40051515](https://crbug.com/40051515),
  [b/159768786](http://b/159768786)
- **TODO:** Drive adoption of the RTP header extension API and remove munging.

#### 5. ICE Options (`kIceOptions` = 20)

- **Use case:** Modifying ICE options (10-20% usage).
- **TODO:** Investigate who is using this and why, then design alternatives.

### Phase 3: Transition & Deprecation

#### TODOs:

- [ ] Implement Chrome console deprecation warnings for all SDP modifications
  when "No modification" metric reaches >90%.
- [ ] Outreach to key developers (Bytedance, libp2p, Meta) regarding ICE
  Ufrag/Pwd deprecation
  ([webrtc:414374642](https://issues.webrtc.org/414374642),
  [webrtc:411871813](https://issues.webrtc.org/411871813)).

## Testing munging effects

### Writing tests that munge SDP

If a unit test or integration test explicitly performs SDP munging (e.g. to test
legacy behavior or error handling), it must declare which munging types it
allows. This is done by configuring the `WebRTC-NoSdpMangleAllowForTesting`
field trial with a comma-separated list of allowed `SdpMungingType` numeric
values.

Example in an integration test:

```cpp
// Allow kBundle (33) munging in this test.
SetFieldTrials("WebRTC-NoSdpMangleAllowForTesting/Enabled,33/");
```

In `sdp_munging_detector_unittest.cc`, use the
`CreateFieldTrialsForMangleTesting` helper, which also ensures that the rollout
blocklist (`WebRTC-NoSdpMangleReject`) is disabled to prevent interference with
the test assertions:

```cpp
CreateFieldTrialsForMangleTesting("WebRTC-NoSdpMangleAllowForTesting/Enabled,1/");
```

### Verifying tests are properly designated

To ensure that all tests performing SDP munging have properly declared their
allowed munges, you can run the test suites with the allow-list enabled but
empty from the command line. This forces rejection of all munging by default.

Run command example:

```bash
out/Default/peerconnection_unittests --force-fieldtrials=WebRTC-NoSdpMangleAllowForTesting/Enabled/
```

Any test that performs SDP munging without explicitly overriding this trial to
allow its specific munge type will fail.

## References

- **Implementation:**
  [pc/sdp_munging_detector.cc](https://cs.chromium.org/chromium/src/third_party/webrtc/pc/sdp_munging_detector.cc?drc=d57bda1bb6222d9606f683fdfc50f0fbe0033f8b)
- **Enum Definition:**
  [SdpMungingType in api/uma_metrics.h](https://cs.chromium.org/chromium/src/third_party/webrtc/api/uma_metrics.h?drc=d57bda1bb6222d9606f683fdfc50f0fbe0033f8b&l=194)
- **Testing Helper:** `CreateFieldTrialsForMangleTesting` in
  [pc/sdp_munging_detector_unittest.cc](https://cs.chromium.org/chromium/src/third_party/webrtc/pc/sdp_munging_detector_unittest.cc?drc=d57bda1bb6222d9606f683fdfc50f0fbe0033f8b)
