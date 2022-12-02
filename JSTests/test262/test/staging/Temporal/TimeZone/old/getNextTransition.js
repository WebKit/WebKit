// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getNextTransition() works as expected
features: [Temporal]
---*/

var noTransitionTZ = Temporal.TimeZone.from("Etc/GMT+10");

// should work for timezones with no scheduled transitions in the near future
var start = Temporal.Instant.from("1945-10-15T13:00:00Z");
assert.sameValue(noTransitionTZ.getNextTransition(start), null);

// accepts string as argument
assert.sameValue(noTransitionTZ.getNextTransition("2019-04-16T21:01Z"), null);
