// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const invalidStrings = [
  "-000000-10-31T17:45Z",
  "-000000-10-31T17:45+00:00[UTC]",
];
const instance = new Temporal.Duration(1);
invalidStrings.forEach((timeZone) => {
  assert.throws(
    RangeError,
    () => instance.total({ unit: "months", relativeTo: { year: 2000, month: 5, day: 2, timeZone } }),
    "reject minus zero as extended year"
  );
  assert.throws(
    RangeError,
    () => instance.total({ unit: "months", relativeTo: { year: 2000, month: 5, day: 2, timeZone: { timeZone } } }),
    "reject minus zero as extended year (nested property)"
  );
});
