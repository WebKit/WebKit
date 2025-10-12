// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

// Lunisolar/lunar calendars can't accurately predict celestial orbits for dates
// far into the past/future. 
//
// https://github.com/unicode-org/icu4x/issues/4917
// https://github.com/tc39/proposal-temporal/issues/2869
const calendarsNotSupportingLargeDates = new Set([
  "chinese",
  "dangi",
  "islamic",
  "islamic-rgsa",
  "islamic-umalqura",
]);

const minDate = new Temporal.PlainDate(-271821, 4, 19);
const maxDate = new Temporal.PlainDate(275760, 9, 13);

for (let calendar of Intl.supportedValuesOf("calendar")) {
  // Skip this calendar when large dates are unsupported to avoid hitting
  // assertions in ICU4X code.
  if (calendarsNotSupportingLargeDates.has(calendar)) {
    continue;
  }

  let min = minDate.withCalendar(calendar);
  let max = maxDate.withCalendar(calendar);

  assert.sameValue(min.year < max.year, true);
}

