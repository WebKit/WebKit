<!-- go/cmark -->

<!--* freshness: {owner: 'danilchap' reviewed: '2026-06-17'} *-->

# SRTP in WebRTC

WebRTC mandates encryption of media by means of the Secure Realtime Protocol, or
SRTP, which is described in
[RFC 3711](https://datatracker.ietf.org/doc/html/rfc3711).

The key negotiation in WebRTC happens using DTLS-SRTP which is described in
[RFC 5764](https://datatracker.ietf.org/doc/html/rfc5764). The legacy
[SDES protocol](https://datatracker.ietf.org/doc/html/rfc4568) is no longer
supported.

Unencrypted RTP can be enabled for debugging purposes by setting the
PeerConnections [`disable_encryption`][1] option to true.

## Supported cipher suites

The implementation supports the following cipher suites:

- SRTP_AES128_CM_HMAC_SHA1_80
- SRTP_AEAD_AES_128_GCM
- SRTP_AEAD_AES_256_GCM

The SRTP_AES128_CM_HMAC_SHA1_32 cipher suite is not enabled by default and off
in Chromium. When enabled, it is accepted for audio-only connections if offered
by the other side. See [`webrtc::CryptoOptions`][2] for details on how to enable
it.

The cipher suite ordering allows a non-WebRTC peer to prefer GCM cipher suites,
however they are not selected as default by two instances of the WebRTC library.

## webrtc::SrtpSession

The [`webrtc::SrtpSession`][3] is providing encryption and decryption of SRTP
packets using [`libsrtp`](https://github.com/cisco/libsrtp). Keys will be
provided by `SrtpTransport` or `DtlsSrtpTransport` in the [`SetSend`][4] and
[`SetReceive`][5] methods.

Encryption and decryption happens in-place in the [`ProtectRtp`][6],
[`ProtectRtcp`][7], [`UnprotectRtp`][8] and [`UnprotectRtcp`][9] methods. The
`SrtpSession` class also takes care of initializing and deinitializing `libsrtp`
by keeping track of how many instances are being used.

## webrtc::SrtpTransport and webrtc::DtlsSrtpTransport

The [`webrtc::SrtpTransport`][10] class is controlling the `SrtpSession`
instances for RTP and RTCP. When
[rtcp-mux](https://datatracker.ietf.org/doc/html/rfc5761) is used, the
`SrtpSession` for RTCP is not needed.

[`webrtc:DtlsSrtpTransport`][11] is a subclass of the `SrtpTransport` that
extracts the keying material when the DTLS handshake is done and configures it
in its base class. It will also become writable only once the DTLS handshake is
done.

[1]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/peer_connection_interface.h;l=1511;drc=031d798a20cc5d9c867598a38632accb20c6effe
[2]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/crypto/crypto_options.h;l=28;drc=031d798a20cc5d9c867598a38632accb20c6effe
[3]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=39;drc=031d798a20cc5d9c867598a38632accb20c6effe
[4]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=53;drc=031d798a20cc5d9c867598a38632accb20c6effe
[5]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=62;drc=031d798a20cc5d9c867598a38632accb20c6effe
[6]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=71;drc=031d798a20cc5d9c867598a38632accb20c6effe
[7]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=74;drc=031d798a20cc5d9c867598a38632accb20c6effe
[8]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=77;drc=031d798a20cc5d9c867598a38632accb20c6effe
[9]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=78;drc=031d798a20cc5d9c867598a38632accb20c6effe
[10]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_transport.h;l=39;drc=031d798a20cc5d9c867598a38632accb20c6effe
[11]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/dtls_srtp_transport.h;l=33;drc=031d798a20cc5d9c867598a38632accb20c6effe
