// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Islamic calendar "islamic-rgsa".
features: [Temporal]
---*/

const calendar = "islamic-rgsa";

// Test data obtained from ICU

const choices = [
  // Approximations of the observational Islamic calendar as computed by ICU4C.
  {
    inLeapYear: false,
    daysInYear: 354,
    daysInMonth12: 29,
    isoDate: "2023-07-18",
  },

  // Approximations of the observational Islamic calendar as computed by ICU4X.
  {
    inLeapYear: true,
    daysInYear: 355,
    daysInMonth12: 30,
    isoDate: "2023-07-19",
  }
];

const year = 1445;
const date = Temporal.PlainDate.from({ year, month: 1, day: 1, calendar });
assert.sameValue(date.calendarId, calendar);
assert.sameValue(date.year, year);
assert.sameValue(date.month, 1);
assert.sameValue(date.monthCode, "M01");
assert.sameValue(date.day, 1);

// Match the possible choice by comparing the ISO date value.
const choice = choices.find(({ isoDate }) => {
  return date.toString().startsWith(isoDate);
});
assert(choice !== undefined, `No applicable choice found for calendar: ${calendar}`);

const { inLeapYear, daysInYear, daysInMonth12, isoDate } = choice;

assert.sameValue(date.inLeapYear, inLeapYear);
assert.sameValue(date.daysInYear, daysInYear);
assert.sameValue(date.monthsInYear, 12);
assert.sameValue(date.dayOfYear, 1);

const startOfNextYear = date.with({ year: year + 1 });
const lastDayOfThisYear = startOfNextYear.subtract({ days: 1 });
assert.sameValue(lastDayOfThisYear.dayOfYear, daysInYear);

const dateMonth12 = date.with({ month: 12 });
assert.sameValue(dateMonth12.daysInMonth, daysInMonth12);
assert.sameValue(date.toString(), `${isoDate}[u-ca=${calendar}]`, "ISO reference date");
