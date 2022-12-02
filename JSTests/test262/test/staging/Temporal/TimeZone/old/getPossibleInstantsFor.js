// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getPossibleInstantsFor() works as expected
includes: [deepEqual.js, temporalHelpers.js]
features: [Temporal]
---*/


// with constant offset
var zone = Temporal.TimeZone.from("+03:30");
var dt = Temporal.PlainDateTime.from("2019-02-16T23:45");
assert.deepEqual(zone.getPossibleInstantsFor(dt).map(a => `${ a }`), ["2019-02-16T20:15:00Z"]);

// with clock moving forward
var zone = TemporalHelpers.springForwardFallBackTimeZone();
var dt = Temporal.PlainDateTime.from("2000-04-02T02:45");
assert.deepEqual(zone.getPossibleInstantsFor(dt), []);

// with clock moving backward
var dt = Temporal.PlainDateTime.from("2000-10-29T01:45");
assert.deepEqual(zone.getPossibleInstantsFor(dt).map(a => `${ a }`), [
  "2000-10-29T08:45:00Z",
  "2000-10-29T09:45:00Z"
]);

// casts argument
var tz = Temporal.TimeZone.from("+03:30");
assert.deepEqual(tz.getPossibleInstantsFor({
  year: 2019,
  month: 2,
  day: 16,
  hour: 23,
  minute: 45,
  second: 30
}).map(a => `${ a }`), ["2019-02-16T20:15:30Z"]);
assert.deepEqual(tz.getPossibleInstantsFor("2019-02-16T23:45:30").map(a => `${ a }`), ["2019-02-16T20:15:30Z"]);

// object must contain at least the required properties
var tz = Temporal.TimeZone.from("UTC");
assert.throws(TypeError, () => tz.getPossibleInstantsFor({ year: 2019 }));
