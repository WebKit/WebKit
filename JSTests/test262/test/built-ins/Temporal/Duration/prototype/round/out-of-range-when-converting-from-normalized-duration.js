// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
    When converting the result from normalized duration form, each duration
    component is turned into a float64-representable integer
features: [Temporal]
---*/

const d = new Temporal.Duration(0, 0, 0, 0, 0, 0, /* s = */ Number.MAX_SAFE_INTEGER, 0, 0, /* ns = */ 999_999_999);
assert.throws(RangeError, () => d.round({
  largestUnit: "nanoseconds",
  roundingIncrement: 1,
}), "nanoseconds component after balancing as a float64-representable integer is out of range");
