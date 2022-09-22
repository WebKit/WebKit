// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getPreviousTransition() works
features: [Temporal]
---*/

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
