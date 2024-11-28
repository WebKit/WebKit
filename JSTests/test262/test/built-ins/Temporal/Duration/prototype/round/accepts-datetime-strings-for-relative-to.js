// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: round() accepts datetime strings or fields for relativeTo.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const d = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);

[
  "2020-01-01",
  "20200101",
  "2020-01-01T00:00:00.000000000",
  {
    year: 2020,
    month: 1,
    day: 1
  }
].forEach(relativeTo => {
    TemporalHelpers.assertDuration(d.round({
        smallestUnit: "seconds",
        relativeTo
    }), 5, 6, 0, 10, 5, 5, 5, 0, 0, 0)
});

