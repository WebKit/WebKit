<!-- go/cmark -->

<!--* freshness: {owner: 'tommi' reviewed: '2026-09-03'} *-->

# WebRTC incoming RTP packet flow

This document details the flow of incoming (receive) RTP packets across the
WebRTC codebase, tracing the path from the network socket to the video receive
stream.

## Threading Model

- **Network Thread**: The entire receive, demuxing, and packet delivery flow
  occurs synchronously on the network thread to minimize delays (webrtc:11993).
- **Worker / Decoder Threads**: Frame decoding, media processing, and playback
  dispatch.

______________________________________________________________________

## Call Stack and Component Flow

### 1. Network / Socket Layer 🌐 *(Network Thread)*

Packets arrive from the network via UDP or TCP sockets. Platform-specific socket
implementations trigger an event when bytes are ready to be read.

- **`AsyncUDPSocket` / `PhysicalSocket`**
  - `OnReadEvent`

### 2. Transport Routing 🛤️ *(Network Thread)*

The bytes move up through the ICE/STUN/TURN connection channels and DTLS layer.

- **`P2PTransportChannel`** -> `OnReadPacket`
  - Routes via **`Connection::OnReadPacket`**
- **`DtlsTransport`** -> `OnReadPacket`
  - Decrypts SRTP packets into cleartext bytes if necessary.

### 3. RTP Transport & Parsing 📦 *(Network Thread)*

The raw bytes reach the RTP core structures where they are parsed into proper
C++ WebRTC objects.

- **`RtpTransport::OnRtpPacketReceived(const ReceivedIpPacket&)`**
  - Converts the byte buffer string into a `RtpPacketReceived`.
  - Extracts header extensions based on the active `RtpHeaderExtensionMap`.
- **`RtpTransport::DemuxPacket`**
  - Passes the parsed object down to the demuxer tree.

### 4. RTP Demuxer (Level 1) 🔀 *(Network Thread)*

- **`RtpDemuxer::OnRtpPacket(const RtpPacketReceived&)`**
  - **Demuxing Happens Here**: This demuxer maps the packet using rules like
    **MID**, **RSID**, or **SSRC**.
  - Routes the packet to the matched `RtpPacketSinkInterface` (usually a
    `BaseChannel`).

### 5. BaseChannel & Media Layer 📺 *(Network Thread)*

- **`BaseChannel::OnRtpPacket(const RtpPacketReceived&)`**
  - Implements `RtpPacketSinkInterface`. Serves as the base conduit for specific
    Media Channels (audio or video).
- **`WebRtcVideoReceiveChannel::OnPacketReceived(RtpPacketReceived)`**
  - Delegates the received packet directly up to the `Call` interface:
    `call_->Receiver()->DeliverRtpPacket(...)`

### 6. Call Interface & Demuxer (Level 2) 📞 *(Network Thread)*

- **`Call::DeliverRtpPacket`**
  - Executes inline on the Network Thread without a thread hop.
  - Employs `RtpStreamReceiverController` to resolve the sink for the SSRC.
  - Passes the packet directly to the appropriate video or audio receive stream.

### 7. Stream Receiver 📥 *(Network Thread)*

- **`RtpVideoStreamReceiver2::OnRtpPacket(const RtpPacketReceived&)`**
  - Executes on the Network Thread.
  - Extracts metadata, processes NACKs/RTCP feedback based on the incoming
    sequence numbers.
  - Inserts the payload into the `PacketBuffer` (Jitter Buffer) to wait for
    frame completion.
  - Once a complete video frame is assembled, it is handed off for decoding.

______________________________________________________________________

## RTP Receive Packet Flow Diagram

```text
       [Network]
           |
           v
+-----------------------+
|   AsyncUDPSocket /    |  (Network Thread)
|   PhysicalSocket      |
+-----------------------+
           |
           v
+-----------------------+
|  P2PTransportChannel  |  (Network Thread)
|  / DtlsTransport      |
+-----------------------+
           |
           v
+-----------------------+
|     RtpTransport      |  (Network Thread)
+-----------------------+
           |
           v
+-----------------------+
|      RtpDemuxer       |  (Network Thread)
+-----------------------+
           |
           v
+-----------------------+
|     BaseChannel       |  (Network Thread)
|  & MediaReceiveChannel|
+-----------------------+
           |
           v
+-----------------------+
| Call::DeliverRtpPacket|  (Network Thread)
+-----------------------+
           |
           v
+-----------------------+
| Stream Receiver       |  (Network Thread)
| (e.g., RtpVideo-      |
|  StreamReceiver2)     |
+-----------------------+
           |
           v
      [Decoder]
```
