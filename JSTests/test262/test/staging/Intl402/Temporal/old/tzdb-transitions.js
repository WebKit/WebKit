// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Tests that are impractical to do without a TZDB
features: [Temporal]
---*/

// getNextTransition()

var nyc = Temporal.TimeZone.from("America/New_York");

// should not have bug #510
var a1 = Temporal.Instant.from("2019-04-16T21:01Z");
var a2 = Temporal.Instant.from("1800-01-01T00:00Z");
assert.sameValue(nyc.getNextTransition(a1).toString(), "2019-11-03T06:00:00Z");
assert.sameValue(nyc.getNextTransition(a2).toString(), "1883-11-18T17:00:00Z");

// should not return the same as its input if the input is a transition point
var inst = Temporal.Instant.from("2019-01-01T00:00Z");
assert.sameValue(`${ nyc.getNextTransition(inst) }`, "2019-03-10T07:00:00Z");
assert.sameValue(`${ nyc.getNextTransition(nyc.getNextTransition(inst)) }`, "2019-11-03T06:00:00Z");

// casts argument
assert.sameValue(`${ nyc.getNextTransition("2019-04-16T21:01Z") }`, "2019-11-03T06:00:00Z");

// getPreviousTransition()

var london = Temporal.TimeZone.from("Europe/London");

// should return first and last transition
var a1 = Temporal.Instant.from("2020-06-11T21:01Z");
var a2 = Temporal.Instant.from("1848-01-01T00:00Z");
assert.sameValue(london.getPreviousTransition(a1).toString(), "2020-03-29T01:00:00Z");
assert.sameValue(london.getPreviousTransition(a2).toString(), "1847-12-01T00:01:15Z");

// should not return the same as its input if the input is a transition point
var inst = Temporal.Instant.from("2020-06-01T00:00Z");
assert.sameValue(`${ london.getPreviousTransition(inst) }`, "2020-03-29T01:00:00Z");
assert.sameValue(`${ london.getPreviousTransition(london.getPreviousTransition(inst)) }`, "2019-10-27T01:00:00Z");

// casts argument
assert.sameValue(`${ london.getPreviousTransition("2020-06-11T21:01Z") }`, "2020-03-29T01:00:00Z");
