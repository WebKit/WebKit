<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2021-05-13'} *-->

# SRTP in WebRTC

WebRTC mandates encryption of media by means of the Secure Realtime Protocol, or
SRTP, which is described in
[RFC 3711](https://datatracker.ietf.org/doc/html/rfc3711).

The key negotiation in WebRTC happens using DTLS-SRTP which is described in
[RFC 5764](https://datatracker.ietf.org/doc/html/rfc5764). The older
[SDES protocol](https://datatracker.ietf.org/doc/html/rfc4568) is implemented
but not enabled by default.

Unencrypted RTP can be enabled for debugging purposes by setting the
PeerConnections [`disable_encryption`][1] option to true.

## Supported cipher suites

The implementation supports the following cipher suites:

*   SRTP_AES128_CM_HMAC_SHA1_80
*   SRTP_AEAD_AES_128_GCM
*   SRTP_AEAD_AES_256_GCM

The SRTP_AES128_CM_HMAC_SHA1_32 cipher suite is accepted for audio-only
connections if offered by the other side. It is not actively supported, see
[SelectCrypto][2] for details.

The cipher suite ordering allows a non-WebRTC peer to prefer GCM cipher suites,
however they are not selected as default by two instances of the WebRTC library.

## cricket::SrtpSession

The [`cricket::SrtpSession`][3] is providing encryption and decryption of SRTP
packets using [`libsrtp`](https://github.com/cisco/libsrtp). Keys will be
provided by `SrtpTransport` or `DtlsSrtpTransport` in the [`SetSend`][4] and
[`SetRecv`][5] methods.

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

## cricket::SrtpFilter

The [`cricket::SrtpFilter`][12] class is used to negotiate SDES.

[1]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/peer_connection_interface.h;l=1413;drc=f467b445631189557d44de86a77ca6a0c3e2108d
[2]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/media_session.cc;l=297;drc=3ac73bd0aa5322abee98f1ff8705af64a184bf61
[3]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=33;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[4]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=40;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[5]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=51;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[6]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=62;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[7]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=69;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[8]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=72;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[9]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.h;l=73;drc=be66d95ab7f9428028806bbf66cb83800bda9241
[10]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_transport.h;l=37;drc=a4d873786f10eedd72de25ad0d94ad7c53c1f68a
[11]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/dtls_srtp_transport.h;l=31;drc=2f8e0536eb97ce2131e7a74e3ca06077aa0b64b3
[12]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_filter.h;drc=d15a575ec3528c252419149d35977e55269d8a41
