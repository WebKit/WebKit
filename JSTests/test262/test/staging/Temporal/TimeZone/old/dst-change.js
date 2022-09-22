// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: with DST change
features: [Temporal]
---*/

// clock moving forward
var zone = new Temporal.TimeZone("Europe/Berlin");
var dtm = new Temporal.PlainDateTime(2019, 3, 31, 2, 45);
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2019-03-31T01:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "earlier" }) }`, "2019-03-31T00:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "later" }) }`, "2019-03-31T01:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "compatible" }) }`, "2019-03-31T01:45:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation: "reject" }));

// clock moving backward
var zone = new Temporal.TimeZone("America/Sao_Paulo");
var dtm = new Temporal.PlainDateTime(2019, 2, 16, 23, 45);
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2019-02-17T01:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "earlier" }) }`, "2019-02-17T01:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "later" }) }`, "2019-02-17T02:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "compatible" }) }`, "2019-02-17T01:45:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation: "reject" }));
