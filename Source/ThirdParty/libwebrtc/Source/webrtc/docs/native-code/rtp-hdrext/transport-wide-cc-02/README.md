# Transport-Wide Congestion Control

This RTP header extension is an extended version of the extension defined in
<https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01>

**Name:** "Transport-wide congenstion control 02"

**Formal name:**
<http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02>

**Status:** This extension is defined here to allow for experimentation. Once
experience has shown that it is useful, we intend to make a proposal based on
it for standardization in the IETF.

The original extension defines a transport-wide sequence number that is used in
feedback packets for congestion control. The original implementation sends these
feedback packets at a periodic interval. The extended version presented here has
two changes compared to the original version:
* Feedback is sent only on request by the sender, therefore, the extension has
  two optional bytes that signals that a feedback packet is requested.
* The sender determines if timing information should be included or not in the
  feedback packet. The original version always include timing information.

Contact <kron@google.com> or <sprang@google.com> for more info.

## RTP header extension format

### Data layout overview
Data layout of transport-wide sequence number
     1-byte header + 2 bytes of data:

      0                   1                   2
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ID   | L=1   |transport-wide sequence number |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Data layout of transport-wide sequence number and optional feedback request
     1-byte header + 4 bytes of data:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ID   | L=3   |transport-wide sequence number |T|  seq count  |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |seq count cont.|
     +-+-+-+-+-+-+-+-+

### Data layout details
The data is written in the following order,
* transport-wide sequence number (16-bit unsigned integer)
* feedback request (optional) (16-bit unsigned integer)<br>
  If the extension contains two extra bytes for feedback request, this means
  that a feedback packet should be generated and sent immediately. The feedback
  request consists of a one-bit field giving the flag value T and a 15-bit
  field giving the sequence count as an unsigned number.
  - If the bit T is set the feedback packet must contain timing information.
  - seq count specifies how many packets of history that should be included in
    the feedback packet. If seq count is zero no feedback should be be
    generated, which is equivalent of sending the two-byte extension above.
    This is added as an option to allow for a fixed packet header size.

## Usage and compatibility

Since the transport-wide CC extension is designed to cover all RTP data
on a connection, it should be specified on all media sections in an SDP
description. However, omitting it for audio has been supported in the past;
in this case, congestion control will ignore the audio.

The transport-wide CC extension cannot be used at the same time as the
CCFB feedback format (RFC 8888). It is legal to make an offer that gives
both transport-wide CC and CCFB, but in an answer, only one of them can
be specified, and that choice has to be consistent across all media sections.
