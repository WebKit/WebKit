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
    isoDate: "2016-03-20",
  },
  {
    testYear: 1396,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoDate: "2017-03-21",
  },
  {
    testYear: 1397,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoDate: "2018-03-21",
  },
  {
    testYear: 1398,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoDate: "2019-03-21",
  },
  {
    testYear: 1399,
    inLeapYear: true,
    daysInYear: 366,
    daysInMonth12: 30,
    isoDate: "2020-03-20",
  },
  {
    testYear: 1400,
    inLeapYear: false,
    daysInYear: 365,
    daysInMonth12: 29,
    isoDate: "2021-03-21",
  }
];

for (const test of tests) {
  const { testYear, inLeapYear, daysInYear, daysInMonth12, isoDate } = test;
  const date = Temporal.PlainDate.from({ year: testYear, month: 1, day: 1, calendar: "persian" });
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
  assert.sameValue(date.toString(), `${isoDate}[u-ca=persian]`, "ISO reference date");
}
