// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Persian calendar
features: [Temporal]
---*/

// Test data obtained from ICU

const tests = [
  {
    testYear: 1395,
    inLeapYear: true,
    daysInYear: 366,
    daysInMonth12: 30,
    isoYear: 2016,
    isoMonth: 3,
    isoDay: 20
  },
  {
    testYear: 1396,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoYear: 2017,
    isoMonth: 3,
    isoDay: 21
  },
  {
    testYear: 1397,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoYear: 2018,
    isoMonth: 3,
    isoDay: 21
  },
  {
    testYear: 1398,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoYear: 2019,
    isoMonth: 3,
    isoDay: 21
  },
  {
    testYear: 1399,
    inLeapYear: true,
    daysInYear: 366,
    daysInMonth12: 30,
    isoYear: 2020,
    isoMonth: 3,
    isoDay: 20
  },
  {
    testYear: 1400,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoYear: 2021,
    isoMonth: 3,
    isoDay: 21
  }
];

for (const test of tests) {
  const { testYear, inLeapYear, daysInYear, daysInMonth12, isoYear, isoMonth, isoDay } = test;
  const date = Temporal.PlainDate.from({ year: testYear, month: 1, day: 1, calendar: "persian" });
  const isoFields = date.getISOFields();
  assert.sameValue(date.calendarId, "persian");
  assert.sameValue(date.year, testYear);
  assert.sameValue(date.month, 1);
  assert.sameValue(date.monthCode, "M01");
  assert.sameValue(date.day, 1);
  assert.sameValue(date.inLeapYear, inLeapYear);
  assert.sameValue(date.daysInYear, daysInYear);
  assert.sameValue(date.monthsInYear, 12);
  assert.sameValue(date.dayOfYear, 1);
  const startOfNextYear = date.with({ year: testYear + 1 });
  const lastDayOfThisYear = startOfNextYear.subtract({ days: 1 });
  assert.sameValue(lastDayOfThisYear.dayOfYear, daysInYear);
  const dateMonth12 = date.with({ month: 12 });
  assert.sameValue(dateMonth12.daysInMonth, daysInMonth12);
  assert.sameValue(isoYear, isoFields.isoYear);
  assert.sameValue(isoMonth, isoFields.isoMonth);
  assert.sameValue(isoDay, isoFields.isoDay);
}
