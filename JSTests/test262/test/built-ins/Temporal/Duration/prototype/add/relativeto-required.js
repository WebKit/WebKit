// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: relativeTo is required if the largest unit is at least weeks.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const d = Temporal.Duration.from({ hours: 1 });
const dy = Temporal.Duration.from({ years: 1 });
const dm = Temporal.Duration.from({ months: 1 });
const dw = Temporal.Duration.from({ weeks: 1 });
assert.throws(RangeError, () => d.add(dy));
assert.throws(RangeError, () => d.add(dm));
assert.throws(RangeError, () => d.add(dw));
assert.throws(RangeError, () => dy.add(d));
assert.throws(RangeError, () => dm.add(d));
assert.throws(RangeError, () => dw.add(d));
const relativeTo = Temporal.PlainDate.from("2000-01-01");
TemporalHelpers.assertDuration(d.add(dy, { relativeTo }),
  1, 0, 0, 0, 1, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(d.add(dm, { relativeTo }),
  0, 1, 0, 0, 1, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(d.add(dw, { relativeTo }),
  0, 0, 1, 0, 1, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(dy.add(d, { relativeTo }),
  1, 0, 0, 0, 1, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(dm.add(d, { relativeTo }),
  0, 1, 0, 0, 1, 0, 0, 0, 0, 0);
TemporalHelpers.assertDuration(dw.add(d, { relativeTo }),
  0, 0, 1, 0, 1, 0, 0, 0, 0, 0);
