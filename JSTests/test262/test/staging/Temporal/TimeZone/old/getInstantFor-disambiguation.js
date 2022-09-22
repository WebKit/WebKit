// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: getInstantFor disambiguation
features: [Temporal]
---*/

var dtm = new Temporal.PlainDateTime(2019, 2, 16, 23, 45);

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
var zone = Temporal.TimeZone.from("America/Sao_Paulo");
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2019-02-17T01:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "earlier" }) }`, "2019-02-17T01:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "later" }) }`, "2019-02-17T02:45:00Z");
assert.sameValue(`${ zone.getInstantFor(dtm, { disambiguation: "compatible" }) }`, "2019-02-17T01:45:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation: "reject" }));

// with daylight saving change - Spring
var dtmLA = new Temporal.PlainDateTime(2020, 3, 8, 2, 30);
var zone = Temporal.TimeZone.from("America/Los_Angeles");
assert.sameValue(`${ zone.getInstantFor(dtmLA) }`, "2020-03-08T10:30:00Z");
assert.sameValue(`${ zone.getInstantFor(dtmLA, { disambiguation: "earlier" }) }`, "2020-03-08T09:30:00Z");
assert.sameValue(`${ zone.getInstantFor(dtmLA, { disambiguation: "later" }) }`, "2020-03-08T10:30:00Z");
assert.sameValue(`${ zone.getInstantFor(dtmLA, { disambiguation: "compatible" }) }`, "2020-03-08T10:30:00Z");
assert.throws(RangeError, () => zone.getInstantFor(dtmLA, { disambiguation: "reject" }));

// throws on bad disambiguation
var zone = Temporal.TimeZone.from("+03:30");
[
  "",
  "EARLIER",
  "test",
].forEach(disambiguation => assert.throws(RangeError, () => zone.getInstantFor(dtm, { disambiguation })));
