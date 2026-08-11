# `resources/video/timing/simulator`

A set of RtcEventLog's recorded using `chrome://webrtc-internals`:

* `video_recv_vp8_pt96`: Receiving a single stream of VP8 on payload type 96.
* `video_recv_vp8_pt96_lossy`: Similar to the previous one, but after half the session there is packet loss.
* `video_recv_vp9_pt98`: Receiving a single stream of VP9 on payload type 98.
* `video_recv_av1_pt45`: Receiving a single stream of AV1 on payload type 45.
* `video_recv_sequential_join_vp8_vp9_av1`: Sequential join of three remote streams, with codecs and payload types as above.

The `video_recv_vp8_pt96_lossy` log was recorded after the RTX OSN logging change in https://webrtc-review.googlesource.com/c/src/+/442320. The other logs were recorded before.
