// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: with DST change
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// clock moving forward
var zone = TemporalHelpers.springForwardFallBackTimeZone();
var dtm = new Temporal.PlainDateTime(2000, 4, 2, 2, 45);
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2000-04-02T10:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "earlier" }) }`, "2000-04-02T09:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "later" }) }`, "2000-04-02T10:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "compatible" }) }`, "2000-04-02T10:45:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation: "reject" }));

// clock moving backward
var dtm = new Temporal.PlainDateTime(2000, 10, 29, 1, 45);
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2000-10-29T08:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "earlier" }) }`, "2000-10-29T08:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "later" }) }`, "2000-10-29T09:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "compatible" }) }`, "2000-10-29T08:45:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation: "reject" }));
