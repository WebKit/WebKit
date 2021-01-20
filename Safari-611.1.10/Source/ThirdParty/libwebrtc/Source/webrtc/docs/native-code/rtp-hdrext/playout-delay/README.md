# Playout Delay

**Name:** "Playout Delay" ; "RTP Header Extension to control Playout Delay"

**Formal name:** <http://www.webrtc.org/experiments/rtp-hdrext/playout-delay>

**SDP "a= name":** "playout-delay" ; this is also used in client/cloud signaling.

**Status:** This extension is defined here to allow for experimentation. Once experience
has shown that it is useful, we intend to make a proposal based on it for standardization
in the IETF.

## Introduction

On WebRTC, the RTP receiver continuously measures inter-packet delay and evaluates packet jitter. Besides this, an estimated delay for decode and render at the receiver is computed. The jitter buffer, the local time extrapolation and the predicted render time (based on predicted decode and render time) impact the delay on a frame before it is rendered at the receiver.

This document proposes an RTP extension to enable the RTP sender to try and limit the amount of playout delay at the receiver in a certain range. A minimum and maximum delay from the sender provides guidance on the range over which the receiver can smooth out rendering.

Thus, this extension aims to provide the sender’s intent to the receiver on how quickly a frame needs to be rendered.

The following use cases are addressed by this extension:

* Interactive streaming (gaming, remote access): Interactive streaming is highly sensitive to end-to-end latency and any delay in render impacts the end-user experience. These use cases prioritize reducing delay over any smoothing done at the receiver. In these cases, the RTP sender would like to disable all smoothing at receiver (min delay = max delay = 0)
* Movie playback: In some scenarios, the user prefers smooth playback and adaptive delay impacts end-user experience (audio can speed up and slow down). In these cases the sender would like to have a fixed delay at all times (min delay = max delay = K)
* Interactive communication: This is the scenarios where the receiver is best suited to adjust the delay adaptively to minimize latency and at the same time add some smoothing based on jitter prevalent due to network conditions (min delay = K1, max delay = K2)


## MIN and MAX playout delay

The playout delay on a frame represents the amount of delay added to a frame the time it is captured at the sender to the time it is expected to be rendered at the receiver. Thus playout delay is essentially:

Playout delay = ExpectedRenderTime(frame) - ExpectedCaptureTime(frame)

MIN and MAX playout delay in turn represent the minimum and maximum delay that can be seen on a frame. This restriction range is best effort. The receiver is expected to try and meet the range as best as it can.

A value of 0 for example is meaningless from the perspective of actually meeting the suggested delay, but it indicates to the receiver that the frame should be rendered as soon as possible. It is up-to the receiver to decide how to handle a frame when it arrives too late (i.e., whether to simply drop or hand over for rendering as soon as possible).

## RTP header extension format

     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |  ID   | len=2 |       MIN delay       |       MAX delay       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


12 bits for Minimum and Maximum delay. This represents a range of 0 - 40950 milliseconds for minimum and maximum (with a granularity of 10 ms). A granularity of 10 ms is sufficient since we expect the following typical use cases:

* 0 ms: Certain gaming scenarios (likely without audio) where we will want to play the frame as soon as possible. Also, for remote desktop without audio where rendering a frame asap makes sense
* 100/150/200 ms: These could be the max target latency for interactive streaming use cases depending on the actual application (gaming, remoting with audio, interactive scenarios)
* 400 ms: Application that want to ensure a network glitch has very little chance of causing a freeze can start with a minimum delay target that is high enough to deal with network issues. Video streaming is one example.

The header is attached to the RTP packet by the RTP sender when it needs to change the min and max smoothing delay at the receiver. Once the sender is informed that at least one RTP packet which has the min and max details is delivered, it MAY stop providing details on all further RTP packets until another change warrants communicating the details to the receiver again.  This is done as follows:

RTCP feedback to RTP sender includes the highest sequence number that was seen on the RTP receiver. The RTP sender can track the sequence number on the packet that first had the playout delay extension and then stop sending the extension once the received sequence number is greater than the sequence number on the first packet containing the current values playout delay in this extension.
