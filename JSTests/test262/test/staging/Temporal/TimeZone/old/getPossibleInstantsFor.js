// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getPossibleInstantsFor() works as expected
includes: [deepEqual.js]
features: [Temporal]
---*/


// with constant offset
var zone = Temporal.TimeZone.from("+03:30");
var dt = Temporal.PlainDateTime.from("2019-02-16T23:45");
assert.deepEqual(zone.getPossibleInstantsFor(dt).map(a => `${ a }`), ["2019-02-16T20:15:00Z"]);

// with clock moving forward
var zone = Temporal.TimeZone.from("Europe/Berlin");
var dt = Temporal.PlainDateTime.from("2019-03-31T02:45");
assert.deepEqual(zone.getPossibleInstantsFor(dt), []);

// with clock moving backward
var zone = Temporal.TimeZone.from("America/Sao_Paulo");
var dt = Temporal.PlainDateTime.from("2019-02-16T23:45");
assert.deepEqual(zone.getPossibleInstantsFor(dt).map(a => `${ a }`), [
  "2019-02-17T01:45:00Z",
  "2019-02-17T02:45:00Z"
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
var tz = Temporal.TimeZone.from("Europe/Amsterdam");
assert.throws(TypeError, () => tz.getPossibleInstantsFor({ year: 2019 }));
