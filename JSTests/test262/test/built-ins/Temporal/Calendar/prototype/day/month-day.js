// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: >
  Temporal.Calendar.prototype.day will take PlainMonthDay and return
  the value of the day.
info: |
  4. If Type(temporalDateLike) is not Object or temporalDateLike does not have
    an [[InitializedTemporalDate]] or [[InitializedTemporalYearMonth]] internal
    slot, then
    a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
  5. Return ! ISODay(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let monthDay = new Temporal.PlainMonthDay(7, 15);
assert.sameValue(cal.day(monthDay), 15, 'cal.day(new Temporal.PlainMonthDay(7, 15)) must return 15');
