// Copyright (C) 2025 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

// Ignore the result, just make sure we don't crash with a debug assertion.
function WithCalendar(date, calendar) {
  try {
    date.withCalendar(calendar);
  } catch {}
}

function PlainDateFrom(options) {
  try {
    Temporal.PlainDate.from(options);
  } catch {}
}

let min_date_iso = new Temporal.PlainDate(-271821, 4, 19);
let max_date_iso = new Temporal.PlainDate(275760, 9, 13);

// https://github.com/unicode-org/icu4x/issues/4917
PlainDateFrom({calendar: "chinese", year: 21206, month: 1, day: 31});

WithCalendar(min_date_iso, "chinese");
WithCalendar(max_date_iso, "chinese");

WithCalendar(min_date_iso, "dangi");
WithCalendar(max_date_iso, "dangi");

WithCalendar(min_date_iso, "islamic");
WithCalendar(max_date_iso, "islamic");

WithCalendar(min_date_iso, "islamic-umalqura");
WithCalendar(max_date_iso, "islamic-umalqura");

// https://github.com/unicode-org/icu4x/issues/5068
PlainDateFrom({calendar: "islamic", year: -6149, month: 1, day: 31});

