<!-- go/cmark -->

<!--* freshness: {owner: 'danilchap' reviewed: '2026-06-17'} *-->

# SctpTransport

## webrtc::SctpTransport

The
[`webrtc::SctpTransport`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/sctp_transport.h;l=40;drc=031d798a20cc5d9c867598a38632accb20c6effe)
class encapsulates an SCTP association, and exposes a few properties of this
association to the WebRTC user (such as Chrome).

The SctpTransport is used to support Datachannels, as described in the
[WebRTC specification for the Peer-to-peer Data API](https://w3c.github.io/webrtc-pc/#peer-to-peer-data-api).

The public interface
([`webrtc::SctpTransportInterface`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/sctp_transport_interface.h;l=143;drc=031d798a20cc5d9c867598a38632accb20c6effe))
exposes an observer interface where the user can define a callback to be called
whenever the state of an SctpTransport changes; this callback is called on the
network thread (as set during PeerConnectionFactory initialization).

The implementation of this object lives in pc/sctp_transport.{h,cc}, and is
basically a wrapper around a `webrtc::SctpTransportInternal`, hiding its
implementation details and APIs that shouldn't be accessed from the user.

The `webrtc::SctpTransport` is a ref counted object; it should be regarded as
owned by the PeerConnection, and will be closed when the PeerConnection closes,
but the object itself may survive longer than the PeerConnection.

## webrtc::SctpTransportInternal

[`webrtc::SctpTransportInternal`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/media/sctp/sctp_transport_internal.h;l=32;drc=031d798a20cc5d9c867598a38632accb20c6effe)
owns two objects: The SCTP association object and the DTLS transport, which is
the object used to send and receive messages as emitted from or consumed by the
sctp library.

See header files for details.
