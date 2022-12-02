// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: getInstantFor disambiguation
includes: [temporalHelpers.js]
features: [Temporal]
---*/

var dtm = new Temporal.PlainDateTime(2000, 10, 29, 1, 45);

// with constant offset
var zone = Temporal.TimeZone.from("+03:30");
for (var disambiguation of [
    undefined,
    "compatible",
    "earlier",
    "later",
    "reject"
  ]) {
  assert(zone.getInstantFor(dtm, { disambiguation }) instanceof Temporal.Instant);
}

// with daylight saving change - Fall
var zone = TemporalHelpers.springForwardFallBackTimeZone();
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2000-10-29T08:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "earlier" }) }`, "2000-10-29T08:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "later" }) }`, "2000-10-29T09:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "compatible" }) }`, "2000-10-29T08:45:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation: "reject" }));

// with daylight saving change - Spring
var dtmLA = new Temporal.PlainDateTime(2000, 4, 2, 2, 30);
assert.sameValue(`${ zone.getInstantFor(dtmLA) }`, "2000-04-02T10:30:00Z");
assert.sameValue(`${ zone.getInstantFor(dtmLA, { disambiguation: "earlier" }) }`, "2000-04-02T09:30:00Z");
assert.sameValue(`${ zone.getInstantFor(dtmLA, { disambiguation: "later" }) }`, "2000-04-02T10:30:00Z");
assert.sameValue(`${ zone.getInstantFor(dtmLA, { disambiguation: "compatible" }) }`, "2000-04-02T10:30:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtmLA, { disambiguation: "reject" }));

// throws on bad disambiguation
var zone = Temporal.TimeZone.from("+03:30");
[
  "",
  "EARLIER",
  "test",
].forEach(disambiguation => assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation })));
