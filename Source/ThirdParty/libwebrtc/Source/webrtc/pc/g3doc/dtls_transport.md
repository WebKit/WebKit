<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2021-05-07'} *-->

## Overview

WebRTC uses DTLS in two ways:

*   to negotiate keys for SRTP encryption using
    [DTLS-SRTP](https://www.rfc-editor.org/info/rfc5763)
*   as a transport for SCTP which is used by the Datachannel API

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
`cricket::DtlsTransportInternal` and allows registering observers implementing
the `webrtc::DtlsTransportObserverInterface`. The
[`webrtc::DtlsTransportObserverInterface`][2] will provide updates to the
observers, passing around a snapshot of the transports state such as the
connection state, the remote certificate(s) and the SRTP ciphers as
[`DtlsTransportInformation`][3].

## cricket::DtlsTransportInternal

The [`cricket::DtlsTransportInternal`][4] class is an interface. Its
implementation is [`cricket::DtlsTransport`][5]. The `cricket::DtlsTransport`
sends and receives network packets via an ICE transport. It also demultiplexes
DTLS packets and SRTP packets according to the scheme described in
[RFC 5764](https://tools.ietf.org/html/rfc5764#section-5.1.2).

## webrtc::DtlsSrtpTranport

The [`webrtc::DtlsSrtpTransport`][6] class is responsіble for extracting the
SRTP keys after the DTLS handshake as well as protection and unprotection of
SRTP packets via its [`cricket::SrtpSession`][7].

[1]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/dtls_transport.h;l=32;drc=6a55e7307b78edb50f94a1ff1ef8393d58218369
[2]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/dtls_transport_interface.h;l=76;drc=34437d5660a80393d631657329ef74c6538be25a
[3]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/dtls_transport_interface.h;l=41;drc=34437d5660a80393d631657329ef74c6538be25a
[4]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/dtls_transport_internal.h;l=63;drc=34437d5660a80393d631657329ef74c6538be25a
[5]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/p2p/base/dtls_transport.h;l=94;drc=653bab6790ac92c513b7cf4cd3ad59039c589a95
[6]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/dtls_srtp_transport.h;l=31;drc=c32f00ea9ddf3267257fe6b45d4d79c6f6bcb829
[7]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=33;drc=be66d95ab7f9428028806bbf66cb83800bda9241
