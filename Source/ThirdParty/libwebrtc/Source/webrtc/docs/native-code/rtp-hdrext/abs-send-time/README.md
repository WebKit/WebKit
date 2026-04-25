# Absolute Send Time

The Absolute Send Time extension is used to stamp RTP packets with a timestamp
showing the departure time from the system that put this packet on the wire
(or as close to this as we can manage). Contact <solenberg@google.com> for
more info.

Name: "Absolute Sender Time" ; "RTP Header Extension for Absolute Sender Time"

Formal name: <http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time>

SDP "a= name": "abs-send-time" ; this is also used in client/cloud signaling.

Not unlike [RTP with TFRC](http://tools.ietf.org/html/draft-ietf-avt-tfrc-profile-10#section-5)

Wire format: 1-byte extension, 3 bytes of data. total 4 bytes extra per packet
(plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2
byte # of extensions). Will in practice replace the "toffset" extension so we
should see no long term increase in traffic as a result.

Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s
wraparound and 3.8us resolution (one increment for each 477 bytes going out on
a 1Gbps interface).

Relation to NTP timestamps: abs_send_time_24 = (ntp_timestamp_64 >> 14) &
0x00ffffff ; NTP timestamp is 32 bits for whole seconds, 32 bits fraction of
second.

Notes: Packets are time stamped when going out, preferably close to metal.
Intermediate RTP relays (entities possibly altering the stream) should remove
the extension or set its own timestamp.
