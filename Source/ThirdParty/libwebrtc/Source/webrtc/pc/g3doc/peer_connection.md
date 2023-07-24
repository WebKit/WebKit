<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2021-05-07'} *-->

# PeerConnection and friends

The PeerConnection is the C++-level implementation of the Javascript
object "RTCPeerConnection" from the
[WEBRTC specification](https://w3c.github.io/webrtc-pc/).

Like many objects in WebRTC, the PeerConnection is used via a factory and an
observer:

 * PeerConnectionFactory, which is created via a static Create method and takes
   a PeerConnectionFactoryDependencies structure listing such things as
   non-default threads and factories for use by all PeerConnections using
   the same factory. (Using more than one factory should be avoided, since
   it takes more resources.)
 * PeerConnection itself, which is created by the method called
   PeerConnectionFactory::CreatePeerConnectionOrError, and takes a
   PeerConnectionInterface::RTCConfiguration argument, as well as
   a PeerConnectionDependencies (even more factories, plus other stuff).
 * PeerConnectionObserver (a member of PeerConnectionDependencies), which
   contains the functions that will be called on events in the PeerConnection

These types are visible in the API.

## Internal structure of PeerConnection and friends

The PeerConnection is, to a large extent, a "God object" - most things
that are done in WebRTC require a PeerConnection.

Internally, it is divided into several objects, each with its own
responsibilities, all of which are owned by the PeerConnection and live
as long as the PeerConnection:

 * SdpOfferAnswerHandler takes care of negotiating configurations with
   a remote peer, using SDP-formatted descriptions.
 * RtpTransmissionManager takes care of the lists of RtpSenders,
   RtpReceivers and RtpTransceivers that form the heart of the transmission
   service.
 * DataChannelController takes care of managing the PeerConnection's
   DataChannels and its SctpTransport.
 * JsepTransportController takes care of configuring the details of senders
   and receivers.
 * Call does management of overall call state.
 * RtcStatsCollector (and its obsolete sibling, StatsCollector) collects
   statistics from all the objects comprising the PeerConnection when
   requested.

There are a number of other smaller objects that are also owned by
the PeerConnection, but it would take too much space to describe them
all here; please consult the .h files.

PeerConnectionFactory owns an object called ConnectionContext, and a
reference to this is passed to each PeerConnection. It is referenced
via an rtc::scoped_refptr, which means that it is guaranteed to be
alive as long as either the factory or one of the PeerConnections
is using it.

