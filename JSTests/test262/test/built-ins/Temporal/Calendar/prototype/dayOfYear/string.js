// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayofyear
description: >
  Temporal.Calendar.prototype.dayOfYear will take ISO8601 string and
  return the day of year.
info: |
  4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ToISODayOfYear(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.sameValue(
  cal.dayOfYear("2019-01-18"),
  18,
  'cal.dayOfYear("2019-01-18") must return 18'
);
assert.sameValue(
  cal.dayOfYear("2020-02-18"),
  49,
  'cal.dayOfYear("2020-02-18") must return 49'
);
assert.sameValue(
  cal.dayOfYear("2019-12-31"),
  365,
  'cal.dayOfYear("2019-12-31") must return 365'
);
assert.sameValue(
  cal.dayOfYear("2000-12-31"),
  366,
  'cal.dayOfYear("2000-12-31") must return 366'
);
