// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinyear
description: >
  Temporal.Calendar.prototype.daysInYear will take PlainDate and return
  the number of days in a year.
info: |
  4. If Type(temporalDateLike) is not Object or temporalDateLike does not have an [[InitializedTemporalDate]] or [[InitializedTemporalYearMonth]] internal slot, then
    a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ISODaysInYear(temporalDateLike.[[ISOYear]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.sameValue(cal.daysInYear("2019-03-18"), 365, 'cal.daysInYear("2019-03-18") must return 365');
assert.sameValue(cal.daysInYear("2020-03-18"), 366, 'cal.daysInYear("2020-03-18") must return 366');
assert.sameValue(cal.daysInYear("2021-03-18"), 365, 'cal.daysInYear("2021-03-18") must return 365');
assert.sameValue(cal.daysInYear("2022-03-18"), 365, 'cal.daysInYear("2022-03-18") must return 365');
assert.sameValue(cal.daysInYear("2023-03-18"), 365, 'cal.daysInYear("2023-03-18") must return 365');
assert.sameValue(cal.daysInYear("2024-03-18"), 366, 'cal.daysInYear("2024-03-18") must return 366');
assert.sameValue(cal.daysInYear("2025-03-18"), 365, 'cal.daysInYear("2025-03-18") must return 365');
assert.sameValue(cal.daysInYear("2026-03-18"), 365, 'cal.daysInYear("2026-03-18") must return 365');
