// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: An ISO 8601 date string should be converted as input
info: |
  a. Perform ? ToTemporalDate(temporalDateLike).
  5. Return 12𝔽.
features: [Temporal]
---*/

let cal = new Temporal.Calendar("iso8601");

assert.sameValue(cal.monthsInYear("3456-12-20"), 12);
assert.sameValue(cal.monthsInYear("+000998-01-28"), 12);
assert.sameValue(cal.monthsInYear("3456-12-20T03:04:05+00:00[UTC]"), 12);
assert.sameValue(cal.monthsInYear("+000998-01-28T03:04:05+00:00[UTC]"), 12);
