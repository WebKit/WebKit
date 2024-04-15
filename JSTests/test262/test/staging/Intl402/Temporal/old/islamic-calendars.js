// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Islamic calendars (note there are 6 variants)
features: [Temporal]
---*/

// Test data obtained from ICU

const tests = [
  {
    calendar: "islamic",
    inLeapYear: false,
    daysInYear: 354,
    daysInMonth12: 29,
    isoYear: 2023,
    isoMonth: 7,
    isoDay: 18
  },
  {
    calendar: "islamic-umalqura",
    inLeapYear: false,
    daysInYear: 354,
    daysInMonth12: 30,
    isoYear: 2023,
    isoMonth: 7,
    isoDay: 19
  },
  {
    calendar: "islamic-civil",
    inLeapYear: true,
    daysInYear: 355,
    daysInMonth12: 30,
    isoYear: 2023,
    isoMonth: 7,
    isoDay: 19
  },
  {
    calendar: "islamicc", // deprecated version of islamic-civil
    inLeapYear: true,
    daysInYear: 355,
    daysInMonth12: 30,
    isoYear: 2023,
    isoMonth: 7,
    isoDay: 19
  },
  {
    calendar: "islamic-rgsa",
    inLeapYear: false,
    daysInYear: 354,
    daysInMonth12: 29,
    isoYear: 2023,
    isoMonth: 7,
    isoDay: 18
  },
  {
    calendar: "islamic-tbla",
    inLeapYear: true,
    daysInYear: 355,
    daysInMonth12: 30,
    isoYear: 2023,
    isoMonth: 7,
    isoDay: 18
  }
];

for (const test of tests) {
  const { calendar, inLeapYear, daysInYear, daysInMonth12, isoYear, isoMonth, isoDay } = test;
  const year = 1445;
  const date = Temporal.PlainDate.from({ year, month: 1, day: 1, calendar });
  const isoFields = date.getISOFields();
  assert.sameValue(date.calendarId, calendar);
  assert.sameValue(date.year, year);
  assert.sameValue(date.month, 1);
  assert.sameValue(date.monthCode, "M01");
  assert.sameValue(date.day, 1);
  assert.sameValue(date.inLeapYear, inLeapYear);
  assert.sameValue(date.daysInYear, daysInYear);
  assert.sameValue(date.monthsInYear, 12);
  assert.sameValue(date.dayOfYear, 1);
  const startOfNextYear = date.with({ year: year + 1 });
  const lastDayOfThisYear = startOfNextYear.subtract({ days: 1 });
  assert.sameValue(lastDayOfThisYear.dayOfYear, daysInYear);
  const dateMonth12 = date.with({ month: 12 });
  assert.sameValue(dateMonth12.daysInMonth, daysInMonth12);
  assert.sameValue(isoYear, isoFields.isoYear);
  assert.sameValue(isoMonth, isoFields.isoMonth);
  assert.sameValue(isoDay, isoFields.isoDay);
}
