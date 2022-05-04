// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: relativeTo is required if the largest unit is at least weeks.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const d = Temporal.Duration.from({ hours: 1 });
const dy = Temporal.Duration.from({ years: 1, hours: 1 });
const dm = Temporal.Duration.from({ months: 1, hours: 1 });
const dw = Temporal.Duration.from({ weeks: 1, hours: 1 });
assert.throws(RangeError, () => d.subtract(dy));
assert.throws(RangeError, () => d.subtract(dm));
assert.throws(RangeError, () => d.subtract(dw));
assert.throws(RangeError, () => dy.subtract(d));
assert.throws(RangeError, () => dm.subtract(d));
assert.throws(RangeError, () => dw.subtract(d));
const relativeTo = Temporal.PlainDate.from("2000-01-01");
TemporalHelpers.assertDuration(d.subtract(dy, { relativeTo }),
  -1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(d.subtract(dm, { relativeTo }),
  0, -1, 0, 0, 0, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(d.subtract(dw, { relativeTo }),
  0, 0, -1, 0, 0, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(dy.subtract(d, { relativeTo }),
  1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(dm.subtract(d, { relativeTo }),
  0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(dw.subtract(d, { relativeTo }),
  0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
