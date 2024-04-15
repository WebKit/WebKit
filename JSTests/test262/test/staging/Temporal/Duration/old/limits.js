// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: min/max values
features: [Temporal]
---*/

var units = [
  "years",
  "months",
  "weeks",
  "days",
  "hours",
  "minutes",
  "seconds",
  "milliseconds",
  "microseconds",
  "nanoseconds"
];

// minimum is zero
assert.sameValue(`${ new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0) }`, "PT0S");
units.forEach(unit => assert.sameValue(`${ Temporal.Duration.from({ [unit]: 0 }) }`, "PT0S"));
[
  "P0Y",
  "P0M",
  "P0W",
  "P0D",
  "PT0H",
  "PT0M",
  "PT0S"
].forEach(str => assert.sameValue(`${ Temporal.Duration.from(str) }`, "PT0S"));
