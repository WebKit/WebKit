Piggybacking DTLS on Stun is a mechanism whereby we send the initial DTLS
handshake over the STUN exchange that opens up the data path, thereby saving round
trips.

The implementation is in dtls_stun_piggyback_controller.{h,cc} and is tested by
dtls_ice_integration_test.cc.

The state machine is as follows:

Initial state:
CLIENT: TENTATIVE
SERVER: TENTATIVE

A "flight" is a set of one or more DTLS messages (see resp. RFC).

*** DTLS1.2 (RFC 6347) ***

Flight 1: CLIENT => SERVER
SERVER when receiving Flight 1: TENTATIVE => CONFIRMED

Flight 2: SERVER => CLIENT
CLIENT when receiving Flight 2: TENTATIVE => CONFIRMED

Flight 3: CLIENT => SERVER
SERVER when receiving Flight 3: CONFIRMED => PENDING (dtls writable)

Flight 4: SERVER => CLIENT
Client when receiving Flight 4: CONFIRMED => COMPLETE (dtls writable)

SERVER will switch to COMPLETE when one of
- Flight 4 is acked
- It receives decryptable data from CLIENT (i.e. it learns that CLIENT is dtls writable)

*** DTLS1.3 (RFC 9147) ***

Flight 1: CLIENT => SERVER
SERVER when receiving Flight 1: TENTATIVE => CONFIRMED

Flight 2: SERVER => CLIENT
CLIENT when receiving Flight 2: TENTATIVE => PENDING (dtls writable)

Flight 3: CLIENT => SERVER
SERVER when receiving Flight 3: CONFIRMED => COMPLETE (dtls writable)

CLIENT will switch to COMPLETE when one of
- Flight 3 is acked
- It receives decryptable data from SERVER (i.e. it learns that SERVER is dtls writable)
