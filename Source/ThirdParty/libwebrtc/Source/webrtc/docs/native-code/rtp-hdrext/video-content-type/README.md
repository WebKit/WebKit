# Video Content Type

The Video Content Type extension is used to communicate a video content type
from sender to receiver of rtp video stream. Contact <ilnik@google.com> for
more info.

Name: "Video Content Type" ; "RTP Header Extension for Video Content Type"

Formal name: <http://www.webrtc.org/experiments/rtp-hdrext/video-content-type>

SDP "a= name": "video-content-type" ; this is also used in client/cloud signaling.

Wire format: 1-byte extension, 1 bytes of data. total 2 bytes extra per packet
(plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2
byte # of extensions).

Values:

  * 0x00: *Unspecified*. Default value. Treated the same as an absence of an extension.
  * 0x01: *Screenshare*. Video stream is of a screenshare type.

Notes: Extension shoud be present only in the last packet of key-frames. If
attached to other packets it should be ignored. If extension is absent,
*Unspecified* value is assumed.
