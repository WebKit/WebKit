# Video Layers Allocation

The goal of this extension is for a video sender to provide information about
the target bitrate, resolution and frame rate of each scalability layer in order
to aid a selective forwarding middlebox to decide which layer to relay.

**Name:** "Video layers allocation version 0"

**Formal name:**
<http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00>

**Status:** This extension is defined here to allow for experimentation.

In a conference scenario, a video from a single sender may be received by
several recipients with different downlink bandwidth constraints and UI
requirements. To allow this, a sender can send video with several scalability
layers and a middle box can choose a layer to relay for each receiver.

This extension support temporal layers, multiple spatial layers sent on a single
rtp stream (SVC), or independent spatial layers sent on multiple rtp streams
(simulcast).

## RTP header extension format

Note: when including the optional width, height and maximum framerate
fields, the total data length of the extension can exceed 16 bytes
and is sent as a two-byte header extension [1]

### Data layout

```
//                           +-+-+-+-+-+-+-+-+
//                           |RID| NS| sl_bm |
//                           +-+-+-+-+-+-+-+-+
// Spatial layer bitmask     |sl0_bm |sl1_bm |
//   up to 2 bytes           |---------------|
//   when sl_bm == 0         |sl2_bm |sl3_bm |
//                           +-+-+-+-+-+-+-+-+
// Number of temporal layers |#tl|#tl|#tl|#tl|
// per spatial layer         |   |   |   |   |
//                           +-+-+-+-+-+-+-+-+
//  Target bitrate in kpbs   |               |
//   per temporal layer      :      ...      :
//    leb128 encoded         |               |
//                           +-+-+-+-+-+-+-+-+
// Resolution and framerate  |               |
// 5 bytes per spatial layer + width-1 for   +
//      (optional)           | rid=0, sid=0  |
//                           +---------------+
//                           |               |
//                           + height-1 for  +
//                           | rid=0, sid=0  |
//                           +---------------+
//                           | max framerate |
//                           +-+-+-+-+-+-+-+-+
//                           :      ...      :
//                           +-+-+-+-+-+-+-+-+
```

RID: RTP stream index this allocation is sent on, numbered from 0. 2 bits.

NS: Number of RTP streams minus one. 2 bits, thus allowing up-to 4 RTP streams.

sl_bm: BitMask of the active Spatial Layers when same for all RTP streams or 0
otherwise. 4 bits, thus allows up to 4 spatial layers per RTP streams.

slX_bm: BitMask of the active Spatial Layers for RTP stream with index=X.
When NS < 2, takes one byte, otherwise uses two bytes. Zero-padded to byte
alignment.

\#tl: 2-bit value of number of temporal layers-1, thus allowing up-to 4 temporal
layers. Values are stored in ascending order of spatial id. Zero-padded to byte
alignment.

Target bitrate in kbps. Values are stored using leb128 encoding [2]. One value per
temporal layer. Values are stored in (RTP stream id, spatial id, temporal id)
ascending order. All bitrates are total required bitrate to receive the
corresponding layer, i.e. in simulcast mode they include only corresponding
spatial layers, in full-svc all lower spatial layers are included. All lower
temporal layers are also included.

Resolution and framerate. Optional. Presence is inferred from the rtp header
extension size. Encoded (width - 1), 16-bit, (height - 1), 16-bit, max frame
rate 8-bit per spatial layer per RTP stream. Values are stored in (RTP stream
id, spatial id) ascending order. Only sent when the resolution differs from the
last values, the framerate changed by more than 5fps and on key frames.

An empty layer allocation (i.e nothing sent on ssrc) is encoded as
special case with a single 0 byte.

[1] https://www.rfc-editor.org/rfc/rfc8285#section-4.3
[2] https://aomediacodec.github.io/av1-spec/#leb128
