// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.round
description: Basic checks for half-expand rounding mode
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const dt = new Temporal.PlainDateTime(1976, 11, 18, 14, 23, 30, 123, 456, 789);

const incrementOneNearest = {
          "day": [1976, 11, "M11", 19,  0,  0,  0,   0,   0,   0],
         "hour": [1976, 11, "M11", 18, 14,  0,  0,   0,   0,   0],
       "minute": [1976, 11, "M11", 18, 14, 24,  0,   0,   0,   0],
       "second": [1976, 11, "M11", 18, 14, 23, 30,   0,   0,   0],
  "millisecond": [1976, 11, "M11", 18, 14, 23, 30, 123,   0,   0],
  "microsecond": [1976, 11, "M11", 18, 14, 23, 30, 123, 457,   0],
   "nanosecond": [1976, 11, "M11", 18, 14, 23, 30, 123, 456, 789]
};

Object.entries(incrementOneNearest).forEach(([smallestUnit, expected]) => {
  TemporalHelpers.assertPlainDateTime(
    dt.round({ smallestUnit, roundingMode: "halfExpand" }),
    ...expected,
    `rounds to nearest ${smallestUnit} (half-expand)`,
    undefined
  );
});
