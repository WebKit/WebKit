# WebRTC incoming RTP packet flow

This document details the flow of incoming (receive) RTP packets across the WebRTC codebase, tracing the path from the network socket to the video receive stream.

## Threading Model
*   **Network Thread**: The majority of the initial receive and demuxing flow occurs synchronously on the network thread to minimize delays. 
*   **Worker Thread**: Packet processing, dispatching encoded frames/buffers to jitter buffers. Actual decoding tends to happen on different threads however.

---

## Call Stack and Component Flow

### 1. Network / Socket Layer 🌐 *(Network Thread)*
Packets arrive from the network via UDP or TCP sockets. Platform-specific socket implementations trigger an event when bytes are ready to be read.
*   **`AsyncUDPSocket` / `PhysicalSocket`**
    *   `OnReadEvent` 

### 2. Transport Routing 🛤️ *(Network Thread)*
The bytes move up through the ICE/STUN/TURN connection channels and DTLS layer. 
*   **`P2PTransportChannel`** -> `OnReadPacket`
    *   Routes via **`Connection::OnReadPacket`**
*   **`DtlsTransport`** -> `OnReadPacket`
    *   Decrypts SRTP packets into cleartext bytes if necessary.

### 3. RTP Transport & Parsing 📦 *(Network Thread)*
The raw bytes reach the RTP core structures where they are parsed into proper C++ WebRTC objects.
*   **`RtpTransport::OnRtpPacketReceived(const ReceivedIpPacket&)`**
    *   Converts the byte buffer string into a `RtpPacketReceived`.
    *   Extracts header extensions based on the active `RtpHeaderExtensionMap`.
*   **`RtpTransport::DemuxPacket`**
    *   Passes the parsed object down to the demuxer tree.

### 4. RTP Demuxer (Level 1) 🔀 *(Network Thread)*
*   **`RtpDemuxer::OnRtpPacket(const RtpPacketReceived&)`**
    *   **Demuxing Happens Here**: This demuxer maps the packet using rules like **MID**, **RSID**, or **SSRC**.
    *   Routes the packet to the matched `RtpPacketSinkInterface` (usually a `BaseChannel`).

### 5. BaseChannel & Media Layer 📺 *(Network Thread)*
*   **`BaseChannel::OnRtpPacket(const RtpPacketReceived&)`**
    *   Implements `RtpPacketSinkInterface`. Serves as the base conduit for specific Media Channels (audio or video).
*   **`WebRtcVideoReceiveChannel::OnPacketReceived(RtpPacketReceived)`**
    *   Delegates the received packet directly up to the `Call` interface: `call_->Receiver()->DeliverRtpPacket(...)`

### 6. Call Interface & Demuxer (Level 2) 📞 *(Network Thread -> Worker Thread)*
*   **`Call::DeliverRtpPacket`**
    *   Evaluates `receive_time_calculator_` directly on the Network Thread to avoid jitter/delay.
    *   **Thread Hop**: Dispatches a task to the **Worker Thread** and invokes `Call::DeliverRtpPacket_w`.
*   **`Call::DeliverRtpPacket_w`**
    *   Executes on the Worker Thread.
    *   Employs a secondary, internal demuxer logic to find the specific receive stream that matches the SSRC.
    *   Calls the appropriate video or audio receive stream.

### 7. Stream Receiver 📥 *(Worker Thread)*
*   **`RtpVideoStreamReceiver2::OnRtpPacket(const RtpPacketReceived&)`**
    *   Executes on the Worker Thread.
    *   Extracts metadata, processes NACKs/RTCP feedback based on the incoming sequence numbers.
    *   Inserts the payload into the `PacketBuffer` (Jitter Buffer) to wait for frame completion.
    *   Once a complete video frame is assembled (or for deeper stream operations), the `VideoFrame` is decoded by `VCMGenericDecoder`.

---

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
           | Thread Hop
           v
+-----------------------+
| Call::                |  (Worker Thread)
| DeliverRtpPacket_w    |
+-----------------------+
           |
           v
+-----------------------+
| Stream Receiver       |  (Worker Thread)
| (e.g., RtpVideo-      |
|  StreamReceiver2)     |
+-----------------------+
           |
           v
      [Decoder]
```
