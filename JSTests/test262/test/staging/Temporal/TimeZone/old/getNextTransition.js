// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getNextTransition() works as expected
features: [Temporal]
---*/

var nyc = Temporal.TimeZone.from("America/New_York");
var noTransitionTZ = Temporal.TimeZone.from("Etc/GMT+10");

// should not have bug #510
var a1 = Temporal.Instant.from("2019-04-16T21:01Z");
var a2 = Temporal.Instant.from("1800-01-01T00:00Z");
assert.sameValue(nyc.getNextTransition(a1).toString(), "2019-11-03T06:00:00Z");
assert.sameValue(nyc.getNextTransition(a2).toString(), "1883-11-18T17:00:00Z");

// should not return the same as its input if the input is a transition point
var inst = Temporal.Instant.from("2019-01-01T00:00Z");
assert.sameValue(`${ nyc.getNextTransition(inst) }`, "2019-03-10T07:00:00Z");
assert.sameValue(`${ nyc.getNextTransition(nyc.getNextTransition(inst)) }`, "2019-11-03T06:00:00Z");

// should work for timezones with no scheduled transitions in the near future
var start = Temporal.Instant.from("1945-10-15T13:00:00Z");
assert.sameValue(noTransitionTZ.getNextTransition(start), null);

// casts argument
assert.sameValue(`${ nyc.getNextTransition("2019-04-16T21:01Z") }`, "2019-11-03T06:00:00Z");

