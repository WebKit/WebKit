<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2022-10-01'} *-->

# getStats in WebRTC

The WebRTC getStats API is specified in
  https://w3c.github.io/webrtc-stats/
and allows querying information about the current state of a RTCPeerConnection
API and some of its member objects.

## Adding new statistics to Chrome

When adding a new standardized `RTCStatsMember` it is necessary to add
it to the Chrome allowlist
  chrome/test/data/webrtc/peerconnection_getstats.js
before landing the WebRTC change. This mechanism prevents the accidential
addition and exposure of non-standard attributes and is not required for
`RTCNonStandardStatsMember` which is not exposed to the web API.