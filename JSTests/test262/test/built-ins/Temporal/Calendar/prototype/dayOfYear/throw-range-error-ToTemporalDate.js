// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayOfYear
description: >
  Temporal.Calendar.prototype.dayOfYear throws RangeError on
  ToTemporalDate when temporalDateLike is invalid string.
info: |
  4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.throws(RangeError, () => cal.dayOfYear("invalid string"),
    'cal.dayOfYear("invalid string") throws a RangeError exception');
