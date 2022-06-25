// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.month
description: >
  Temporal.Calendar.prototype.month will take PlainDateTime and return
  the value of the month.
info: |
  5. If Type(temporalDateLike) is not Object or temporalDateLike does not have
    an [[InitializedTemporalDate]] or [[InitializedTemporalYearMonth]]
    internal slot, then
    a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
  6. Return ! ISOMonth(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dateTime = new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)
assert.sameValue(cal.month(dateTime), 8, 'cal.month(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)) must return 8');
