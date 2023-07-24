<!-- go/cmark -->
<!--* freshness: {owner: 'jonaso' reviewed: '2021-04-12'} *-->

# ICE

## Overview

ICE ([link](https://developer.mozilla.org/en-US/docs/Glossary/ICE)) provides
unreliable packet transport between two clients (p2p) or between a client and a
server.

This documentation provides an overview of how ICE is implemented, i.e how the
following classes interact.

*   [`cricket::IceTransportInternal`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/ice_transport_internal.h;l=225;drc=8cb97062880b0e0a78f9d578370a01aced81a13f) -
    is the interface that does ICE (manage ports, candidates, connections to
    send/receive packets). The interface is implemented by
    [`cricket::P2PTransportChannel`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/p2p_transport_channel.h;l=103;drc=0ccfbd2de7bc3b237a0f8c30f48666c97b9e5523).

*   [`cricket::PortInterface`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/port_interface.h;l=47;drc=c3a486c41e682cce943f2b20fe987c9421d4b631)
    Represents a local communication mechanism that can be used to create
    connections to similar mechanisms of the other client. There are 4
    implementations of `cricket::PortInterface`
    [`cricket::UDPPort`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/stun_port.h;l=33;drc=a4d873786f10eedd72de25ad0d94ad7c53c1f68a),
    [`cricket::StunPort`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/stun_port.h;l=265;drc=a4d873786f10eedd72de25ad0d94ad7c53c1f68a),
    [`cricket::TcpPort`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/tcp_port.h;l=33;drc=7a284e1614a38286477ed2334ecbdde78e87b79c)
    and
    [`cricket::TurnPort`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/turn_port.h;l=44;drc=ffb7603b6025fbd6e79f360d293ab49092bded54).
    The ports share lots of functionality in a base class,
    [`cricket::Port`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/port.h;l=187;drc=3ba7beba29c4e542c4a9bffcc5a47d5e911865be).

*   [`cricket::Candidate`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/candidate.h;l=30;drc=10542f21c8e4e2d60b136fab45338f2b1e132dde)
    represents an address discovered by a `cricket::Port`. A candidate can be
    local (i.e discovered by a local port) or remote. Remote candidates are
    transported using signaling, i.e outside of webrtc. There are 4 types of
    candidates: `local`, `stun`, `prflx` or `relay`
    ([standard](https://developer.mozilla.org/en-US/docs/Web/API/RTCIceCandidateType))

*   [`cricket::Connection`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/connection.h)
    provides the management of a `cricket::CandidatePair`, i.e for sending data
    between two candidates. It sends STUN Binding requests (aka STUN pings) to
    verify that packets can traverse back and forth and keep connections alive
    (both that NAT binding is kept, and that the remote peer still wants the
    connection to remain open).

*   `cricket::P2PTransportChannel` uses an
    [`cricket::PortAllocator`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/port_allocator.h;l=335;drc=9438fb3fff97c803d1ead34c0e4f223db168526f)
    to create ports and discover local candidates. The `cricket::PortAllocator`
    is implemented by
    [`cricket::BasicPortAllocator`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/client/basic_port_allocator.h;l=29;drc=e27f3dea8293884701283a54f90f8a429ea99505).

*   `cricket::P2PTransportChannel` uses an
    [`cricket::IceControllerInterface`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/ice_controller_interface.h;l=73;drc=9438fb3fff97c803d1ead34c0e4f223db168526f)
    to manage a set of connections. The `cricket::IceControllerInterface`
    decides which `cricket::Connection` to send data on.

## Connection establishment

This section describes a normal sequence of interactions to establish ice state
completed
[ link ](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/ice_transport_internal.h;l=208;drc=9438fb3fff97c803d1ead34c0e4f223db168526f)
([ standard ](https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState))

All of these steps are invoked by interactions with `PeerConnection`.

1.  [`P2PTransportChannel::MaybeStartGathering`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/p2p_transport_channel.cc;l=864;drc=0ccfbd2de7bc3b237a0f8c30f48666c97b9e5523)
    This function is invoked as part of `PeerConnection::SetLocalDescription`.
    `P2PTransportChannel` will use the `cricket::PortAllocator` to create a
    `cricket::PortAllocatorSession`. The `cricket::PortAllocatorSession` will
    create local ports as configured, and the ports will start gathering
    candidates.

2.  [`IceTransportInternal::SignalCandidateGathered`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/ice_transport_internal.h;l=293;drc=8cb97062880b0e0a78f9d578370a01aced81a13f)
    When a port finds a local candidate, it will be added to a list on
    `cricket::P2PTransportChannel` and signaled to application using
    `IceTransportInternal::SignalCandidateGathered`. A p2p application can then
    send them to peer using favorite transport mechanism whereas a client-server
    application will do nothing.

3.  [`P2PTransportChannel::AddRemoteCandidate`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/p2p_transport_channel.cc;l=1233;drc=0ccfbd2de7bc3b237a0f8c30f48666c97b9e5523)
    When the application get a remote candidate, it can add it using
    `PeerConnection::AddRemoteCandidate` (after
    `PeerConnection::SetRemoteDescription` has been called!), this will trickle
    down to `P2PTransportChannel::AddRemoteCandidate`. `P2PTransportChannel`
    will combine the remote candidate with all compatible local candidates to
    form new `cricket::Connection`(s). Candidates are compatible if it is
    possible to send/receive data (e.g ipv4 can only send to ipv4, tcp can only
    connect to tcp etc...) The newly formed `cricket::Connection`(s) will be
    added to the `cricket::IceController` that will decide which
    `cricket::Connection` to send STUN ping on.

4.  [`P2PTransportChannel::SignalCandidatePairChanged`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/ice_transport_internal.h;l=310;drc=8cb97062880b0e0a78f9d578370a01aced81a13f)
    When a remote connection replies to a STUN ping, `cricket::IceController`
    will instruct `P2PTransportChannel` to use the connection. This is signalled
    up the stack using `P2PTransportChannel::SignalCandidatePairChanged`. Note
    that `cricket::IceController` will continue to send STUN pings on the
    selected connection, as well as other connections.

5.  [`P2PTransportChannel::SignalIceTransportStateChanged`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/ice_transport_internal.h;l=323;drc=8cb97062880b0e0a78f9d578370a01aced81a13f)
    The initial selection of a connection makes `P2PTransportChannel` signal up
    stack that state has changed, which may make [`cricket::DtlsTransportInternal`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/dtls_transport_internal.h;l=63;drc=653bab6790ac92c513b7cf4cd3ad59039c589a95)
    initiate a DTLS handshake (depending on the DTLS role).
