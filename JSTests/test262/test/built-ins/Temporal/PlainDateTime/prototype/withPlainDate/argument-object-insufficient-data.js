// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: Unrecognized properties of plain object ignored
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dt = new Temporal.PlainDateTime(1995, 12, 7, 3, 24, 30);

assert.throws(
  TypeError,
  () => dt.withPlainDate({}),
  "empty object not acceptable"
);

assert.throws(
  TypeError,
  () => dt.withPlainDate({ months: 12 }), // should be "month"
  "no recognized properties (look like it might work)"
);

assert.throws(
  TypeError,
  () => dt.with({nonsense: true}),
  "no recognized properties (clearly won't work)"
);

TemporalHelpers.assertPlainDateTime(
  dt.withPlainDate({ year: 2000, month: 6, day: 1, months: 123 }), // 'months' unrecognized; see above
  2000, 6, "M06", 1, 3, 24, 30, 0, 0, 0,
  "unrecognized properties ignored & does not throw if recognized properties present)"
);
