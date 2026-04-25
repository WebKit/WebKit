<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2021-06-03'} *-->

# RTP in WebRTC

WebRTC uses the RTP protocol described in
[RFC3550](https://datatracker.ietf.org/doc/html/rfc3550) for transporting audio
and video. Media is encrypted using [SRTP](./srtp.md).

## Allocation of payload types

RTP packets have a payload type field that describes which media codec can be
used to handle a packet. For some (older) codecs like PCMU the payload type is
assigned statically as described in
[RFC3551](https://datatracker.ietf.org/doc/html/rfc3551). For others, it is
assigned dynamically through the SDP. **Note:** there are no guarantees on the
stability of a payload type assignment.

For this allocation, the range from 96 to 127 is used. When this range is
exhausted, the allocation falls back to the range from 35 to 63 as permitted by
[section 5.1 of RFC3550][1]. Note that older versions of WebRTC failed to
recognize payload types in the lower range. Newer codecs (such as flexfec-03 and
AV1) will by default be allocated in that range.

Payload types in the range 64 to 95 are not used to avoid confusion with RTCP as
described in [RFC5761](https://datatracker.ietf.org/doc/html/rfc5761).

## Allocation of audio payload types

Audio payload types are assigned from a table by the [PayloadTypeMapper][2]
class. New audio codecs should be allocated in the lower dynamic range [35,63],
starting at 63, to reduce collisions with payload types

## Allocation of video payload types

Video payload types are allocated by the
[GetPayloadTypesAndDefaultCodecs method][3]. The set of codecs depends on the
platform, in particular for H264 codecs and their different profiles. Payload
numbers are assigned ascending from 96 for video codecs and their
[associated retransmission format](https://datatracker.ietf.org/doc/html/rfc4588).
Some codecs like flexfec-03 and AV1 are assigned to the lower range [35,63] for
reasons explained above. When the upper range [96,127] is exhausted, payload
types are assigned to the lower range [35,63], starting at 35.

## Handling of payload type collisions

Due to the requirement that payload types must be uniquely identifiable when
using [BUNDLE](https://datatracker.ietf.org/doc/html/rfc8829) collisions between
the assignments of the audio and video payload types may arise. These are
resolved by the [UsedPayloadTypes][4] class which will reassign payload type
numbers descending from 127.

# Bitrate probing

Bandwidth estimation sometimes requires sending RTP packets to ramp up the
estimate above a certain treshold. WebRTC uses two techniques for that:

* Packets that only consist of RTP padding
* RTX packets

At the receiving end, both types of probing packets do not interfere with media processing.
After being taken into account for bandwidth estimation, probing packets only consisting
of padding can be dropped and RTX packets act as redundancy.

Using RTX for probing is generally preferred as padding probes are limited to 256 bytes
of RTP payload which results in a larger number of packets being used for probing which
is a disadvantage from a congestion control point of view.

## Padding probes

Padding probes consist of RTP packets with header extensions (either abs-send time or
the transport-wide-cc sequence number) and set the RTP "P" bit. The last byte of the
RTP payload which specifies the amount of padding is set to 255 and preceeded by 255
bytes of all zeroes. See [section 5.1 of RFC3550][1].
Padding probes use the RTX RTP stream (i.e. payload type, sequence number and timestamp)
when RTX is negotiated or share the same RTP stream as the media packets otherwise.

Padding probes are used either when
* RTX is not negotiated (such as for audio, less commonly for video) or
* no suitable original packet is found for RTX probing.

Padding probes should not be interleaved with packets of a video frame.

## RTX probes

RTX probes are resends of previous packets that use RTX retransmissions specified in
[RFC4588](https://www.rfc-editor.org/rfc/rfc4588) as payload format when negotiated with
the peer. These packets will have a different abs-send-time or transport-wide-cc sequence
number and use the RTX RTP stream (i.e. RTX payload type, sequence number and timestamp)
associated with the media RTP stream.

RTX probing uses recently sent RTP packets that have not yet been acknowledged by
the remote side. Sending these packets again has a small chance of being useful when the
original packet is lost and will not affect RTP processing at the receiver otherwise.

[1]: https://datatracker.ietf.org/doc/html/rfc3550#section-5.1
[2]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/media/engine/payload_type_mapper.cc;l=25;drc=4f26a3c7e8e20e0e0ca4ca67a6ebdf3f5543dc3f
[3]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/media/engine/webrtc_video_engine.cc;l=119;drc=b412efdb780c86e6530493afa403783d14985347
[4]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/used_ids.h;l=94;drc=b412efdb780c86e6530493afa403783d14985347
