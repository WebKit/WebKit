<?% config.freshness.reviewed = '2021-04-12' %?>
<?% config.freshness.owner = 'sprang' %?>

# Paced Sending

The paced sender, often referred to as just the "pacer", is a part of the WebRTC
RTP stack used primarily to smooth the flow of packets sent onto the network.

## Background

Consider a video stream at 5Mbps and 30fps. This would in an ideal world result
in each frame being ~21kB large and packetized into 18 RTP packets. While the
average bitrate over say a one second sliding window would be a correct 5Mbps,
on a shorter time scale it can be seen as a burst of 167Mbps every 33ms, each
followed by a 32ms silent period. Further, it is quite common that video
encoders overshoot the target frame size in case of sudden movement especially
dealing with screensharing. Frames being 10x or even 100x larger than the ideal
size is an all too real scenario. These packet bursts can cause several issues,
such as congesting networks and causing buffer bloat or even packet loss. Most
sessions have more than one media stream, e.g. a video and an audio track. If
you put a frame on the wire in one go, and those packets take 100ms to reach the
other side - that means you have now blocked any audio packets from reaching the
remote end in time as well.

The paced sender solves this by having a buffer in which media is queued, and
then using a _leaky bucket_ algorithm to pace them onto the network. The buffer
contains separate fifo streams for all media tracks so that e.g. audio can be
prioritized over video - and equal prio streams can be sent in a round-robin
fashion to avoid any one stream blocking others.

Since the pacer is in control of the bitrate sent on the wire, it is also used
to generate padding in cases where a minimum send rate is required - and to
generate packet trains if bitrate probing is used.

## Life of a Packet

The typical path for media packets when using the paced sender looks something
like this:

1.  `RTPSenderVideo` or `RTPSenderAudio` packetizes media into RTP packets.
2.  The packets are sent to the [RTPSender] class for transmission.
3.  The pacer is called via [RtpPacketSender] interface to enqueue the packet
    batch.
4.  The packets are put into a queue within the pacer awaiting opportune moments
    to send them.
5.  At a calculated time, the pacer calls the `PacingController::PacketSender()`
    callback method, normally implemented by the [PacketRouter] class.
6.  The router forwards the packet to the correct RTP module based on the
    packet's SSRC, and in which the `RTPSenderEgress` class makes final time
    stamping, potentially records it for retransmissions etc.
7.  The packet is sent to the low-level `Transport` interface, after which it is
    now out of scope.

Asynchronously to this, the estimated available send bandwidth is determined -
and the target send rate is set on the `RtpPacketPacker` via the `void
SetPacingRates(DataRate pacing_rate, DataRate padding_rate)` method.

## Packet Prioritization

The pacer prioritized packets based on two criteria:

*   Packet type, with most to least prioritized:
    1.  Audio
    2.  Retransmissions
    3.  Video and FEC
    4.  Padding
*   Enqueue order

The enqueue order is enforced on a per stream (SSRC) basis. Given equal
priority, the [RoundRobinPacketQueue] alternates between media streams to ensure
no stream needlessly blocks others.

## Implementations

There are currently two implementations of the paced sender (although they share
a large amount of logic via the `PacingController` class). The legacy
[PacedSender] uses a dedicated thread to poll the pacing controller at 5ms
intervals, and has a lock to protect internal state. The newer
[TaskQueuePacedSender] as the name implies uses a TaskQueue to both protect
state and schedule packet processing, the latter is dynamic based on actual send
rates and constraints. Avoid using the legacy PacedSender in new applications as
we are planning to remove it.

## The Packet Router

An adjacent component called [PacketRouter] is used to route packets coming out
of the pacer and into the correct RTP module. It has the following functions:

*   The `SendPacket` method looks up an RTP module with an SSRC corresponding to
    the packet for further routing to the network.
*   If send-side bandwidth estimation is used, it populates the transport-wide
    sequence number extension.
*   Generate padding. Modules supporting payload-based padding are prioritized,
    with the last module to have sent media always being the first choice.
*   Returns any generated FEC after having sent media.
*   Forwards REMB and/or TransportFeedback messages to suitable RTP modules.

At present the FEC is generated on a per SSRC basis, so is always returned from
an RTP module after sending media. Hopefully one day we will support covering
multiple streams with a single FlexFEC stream - and the packet router is the
likely place for that FEC generator to live. It may even be used for FEC padding
as an alternative to RTX.

## The API

The section outlines the classes and methods relevant to a few different use
cases of the pacer.

### Packet sending

For sending packets, use
`RtpPacketSender::EnqueuePackets(std::vector<std::unique_ptr<RtpPacketToSend>>
packets)` The pacer takes a `PacingController::PacketSender` as constructor
argument, this callback is used when it's time to actually send packets.

### Send rates

To control the send rate, use `void SetPacingRates(DataRate pacing_rate,
DataRate padding_rate)` If the packet queue becomes empty and the send rate
drops below `padding_rate`, the pacer will request padding packets from the
`PacketRouter`.

In order to completely suspend/resume sending data (e.g. due to network
availability), use the `Pause()` and `Resume()` methods.

The specified pacing rate may be overriden in some cases, e.g. due to extreme
encoder overshoot. Use `void SetQueueTimeLimit(TimeDelta limit)` to specify the
longest time you want packets to spend waiting in the pacer queue (pausing
excluded). The actual send rate may then be increased past the pacing_rate to
try to make the _average_ queue time less than that requested limit. The
rationale for this is that if the send queue is say longer than three seconds,
it's better to risk packet loss and then try to recover using a key-frame rather
than cause severe delays.

### Bandwidth estimation

If the bandwidth estimator supports bandwidth probing, it may request a cluster
of packets to be sent at a specified rate in order to gauge if this causes
increased delay/loss on the network. Use the `void CreateProbeCluster(DataRate
bitrate, int cluster_id)` method - packets sent via this `PacketRouter` will be
marked with the corresponding cluster_id in the attached `PacedPacketInfo`
struct.

If congestion window pushback is used, the state can be updated using
`SetCongestionWindow()` and `UpdateOutstandingData()`.

A few more methods control how we pace: * `SetAccountForAudioPackets()`
determines if audio packets count into bandwidth consumed. *
`SetIncludeOverhead()` determines if the entire RTP packet size counts into
bandwidth used (otherwise just media payload). * `SetTransportOverhead()` sets
an additional data size consumed per packet, representing e.g. UDP/IP headers.

### Stats

Several methods are used to gather statistics in pacer state:

*   `OldestPacketWaitTime()` time since the oldest packet in the queue was
    added.
*   `QueueSizeData()` total bytes currently in the queue.
*   `FirstSentPacketTime()` absolute time the first packet was sent.
*   `ExpectedQueueTime()` total bytes in the queue divided by the send rate.

[RTPSender]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/rtp_rtcp/source/rtp_sender.h;drc=77ee8542dd35d5143b5788ddf47fb7cdb96eb08e
[RtpPacketSender]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/rtp_rtcp/include/rtp_packet_sender.h;drc=ea55b0872f14faab23a4e5dbcb6956369c8ed5dc
[RtpPacketPacer]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/rtp_packet_pacer.h;drc=e7bc3a347760023dd4840cf6ebdd1e6c8592f4d7
[PacketRouter]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/packet_router.h;drc=3d2210876e31d0bb5c7de88b27fd02ceb1f4e03e
[PacedSender]: https://source.chromium.org/chromium/chromium/src/+/main:media/cast/net/pacing/paced_sender.h;drc=df00acf8f3cea9a947e11dc687aa1147971a1883
[TaskQueuePacedSender]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/task_queue_paced_sender.h;drc=5051693ada61bc7b78855c6fb3fa87a0394fa813
[RoundRobinPacketQueue]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/round_robin_packet_queue.h;drc=b571ff48f8fe07678da5a854cd6c3f5dde02855f
