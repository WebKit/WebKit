// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: >
  The durationLike argument must contain at least one correctly spelled property
features: [Temporal]
---*/

let d = new Temporal.Duration(1, 2, 3, 4, 5);

[
  {},
  [],
  () => {},
  // objects with only singular keys (plural is the correct spelling)
  { year: 1 },
  { month: 2 },
  { week: 3 },
  { day: 4 },
  { hour: 5 },
  { minute: 6 },
  { second: 7 },
  { millisecond: 8 },
  { microsecond: 9 },
  { nanosecond: 10 },
].forEach((badObject) => {
  assert.throws(TypeError, () => d.with(badObject),
    "Throw TypeError if temporalDurationLike is not valid");
});
