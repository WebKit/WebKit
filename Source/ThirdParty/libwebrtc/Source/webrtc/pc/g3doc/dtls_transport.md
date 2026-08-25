<!-- go/cmark -->

<!--* freshness: {owner: 'danilchap' reviewed: '2026-06-17'} *-->

## Overview

WebRTC uses DTLS in two ways:

- to negotiate keys for SRTP encryption using
  [DTLS-SRTP](https://www.rfc-editor.org/info/rfc5763)
- as a transport for SCTP which is used by the Datachannel API

The W3C WebRTC API represents this as the
[DtlsTransport](https://w3c.github.io/webrtc-pc/#rtcdtlstransport-interface).

The DTLS handshake happens after the ICE transport becomes writable and has
found a valid pair. It results in a set of keys being derived for DTLS-SRTP as
well as a fingerprint of the remote certificate which is compared to the one
given in the SDP `a=fingerprint:` line.

This documentation provides an overview of how DTLS is implemented, i.e how the
following classes interact.

## webrtc::DtlsTransport

The [`webrtc::DtlsTransport`][1] class is a wrapper around the
`webrtc::DtlsTransportInternal` and allows registering observers implementing
the `webrtc::DtlsTransportObserverInterface`. The
[`webrtc::DtlsTransportObserverInterface`][2] will provide updates to the
observers, passing around a snapshot of the transports state such as the
connection state, the remote certificate(s) and the SRTP ciphers as
[`DtlsTransportInformation`][3].

## webrtc::DtlsTransportInternal

The [`webrtc::DtlsTransportInternal`][4] class is an interface. Its
implementation is [`webrtc::DtlsTransportInternalImpl`][5]. The
`webrtc::DtlsTransportInternalImpl` sends and receives network packets via an
ICE transport. It also demultiplexes DTLS packets and SRTP packets according to
the scheme described in
[RFC 5764](https://tools.ietf.org/html/rfc5764#section-5.1.2).

## webrtc::DtlsSrtpTranport

The [`webrtc::DtlsSrtpTransport`][6] class is responsіble for extracting the
SRTP keys after the DTLS handshake as well as protection and unprotection of
SRTP packets via its [`webrtc::SrtpSession`][7].

[1]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/dtls_transport.h;l=30;drc=031d798a20cc5d9c867598a38632accb20c6effe
[2]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/dtls_transport_interface.h;l=96;drc=031d798a20cc5d9c867598a38632accb20c6effe
[3]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/dtls_transport_interface.h;l=46;drc=031d798a20cc5d9c867598a38632accb20c6effe
[4]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/dtls/dtls_transport_internal.h;l=49;drc=031d798a20cc5d9c867598a38632accb20c6effe
[5]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/dtls/dtls_transport.h;l=137;drc=031d798a20cc5d9c867598a38632accb20c6effe
[6]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/dtls_srtp_transport.h;l=33;drc=031d798a20cc5d9c867598a38632accb20c6effe
[7]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=39;drc=031d798a20cc5d9c867598a38632accb20c6effe
