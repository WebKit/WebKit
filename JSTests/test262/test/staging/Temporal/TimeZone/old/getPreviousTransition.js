// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getPreviousTransition() works
features: [Temporal]
---*/

var utc = Temporal.TimeZone.from("UTC");

// no transitions without a TZDB
var instant = Temporal.Instant.from("2020-06-11T21:01Z");
assert.sameValue(utc.getPreviousTransition(instant), null);

// accepts string as argument
assert.sameValue(utc.getPreviousTransition("2020-06-11T21:01Z"), null);
