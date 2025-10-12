// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

const months = [
  {
    name: "Tishrei",
    month: [1],
    monthCode: "M01",
    days: [30],
  },
  {
    name: "Cheshvan",
    month: [2],
    monthCode: "M02",
    days: [29, 30],  // Rosh Hashanah postponement rules
  },
  {
    name: "Kislev",
    month: [3],
    monthCode: "M03",
    days: [29, 30],  // Rosh Hashanah postponement rules
  },
  {
    name: "Tevet",
    month: [4],
    monthCode: "M04",
    days: [29],
  },
  {
    name: "Shevat",
    month: [5],
    monthCode: "M05",
    days: [30],
  },
  {
    name: "Adar I",
    month: [6],
    monthCode: "M05L",
    days: [30],
  },
  {
    name: "Adar", // Adar II
    month: [6, 7],
    monthCode: "M06",
    days: [29],
  },
  {
    name: "Nisan",
    month: [7, 8],
    monthCode: "M07",
    days: [30],
  },
  {
    name: "Iyar",
    month: [8, 9],
    monthCode: "M08",
    days: [29],
  },
  {
    name: "Sivan",
    month: [9, 10],
    monthCode: "M09",
    days: [30],
  },
  {
    name: "Tammuz",
    month: [10, 11],
    monthCode: "M10",
    days: [29],
  },
  {
    name: "Av",
    month: [11, 12],
    monthCode: "M11",
    days: [30],
  },
  {
    name: "Elul",
    month: [12, 13],
    monthCode: "M12",
    days: [29],
  },
];

// Test non-leap years.
for (let {month: [month], monthCode, days} of months) {
  // Year 1 isn't a leap year.
  const year = 1;

  // Skip over leap months.
  if (monthCode.endsWith("L")) {
    continue;
  }

  let startOfMonth = Temporal.PlainDate.from({
    calendar: "hebrew",
    year,
    month,
    day: 1,
  });

  assert.sameValue(startOfMonth.year, year);
  assert.sameValue(startOfMonth.month, month);
  assert.sameValue(startOfMonth.monthCode, monthCode);
  assert.sameValue(startOfMonth.day, 1);

  let endOfMonth = Temporal.PlainDate.from({
    calendar: "hebrew",
    year,
    month,
    day: days[0],
  });

  assert.sameValue(endOfMonth.year, year);
  assert.sameValue(endOfMonth.month, month);
  assert.sameValue(endOfMonth.monthCode, monthCode);
  assert.sameValue(endOfMonth.day, days[0]);
}

// Test leap years.
for (let {month: [nonLeapMonth, month = nonLeapMonth], monthCode, days} of months) {
  // Year 3 is a leap year.
  const year = 3;
  
  let startOfMonth = Temporal.PlainDate.from({
    calendar: "hebrew",
    year,
    month,
    day: 1,
  });

  assert.sameValue(startOfMonth.year, year);
  assert.sameValue(startOfMonth.month, month);
  assert.sameValue(startOfMonth.monthCode, monthCode);
  assert.sameValue(startOfMonth.day, 1);

  let endOfMonth = Temporal.PlainDate.from({
    calendar: "hebrew",
    year,
    month,
    day: days[0],
  });

  assert.sameValue(endOfMonth.year, year);
  assert.sameValue(endOfMonth.month, month);
  assert.sameValue(endOfMonth.monthCode, monthCode);
  assert.sameValue(endOfMonth.day, days[0]);
}

// Rosh Hashanah postponement
{
  // Days in Cheshvan and Kislev for years 0..10.
  const daysPerMonth = {
    Cheshvan: [
      29, 30, 30, 29, 29, 30, 30, 29, 29, 30, 29,
    ],
    Kislev: [
      30, 30, 30, 29, 30, 30, 30, 30, 29, 30, 30,
    ],
  };

  for (let year = 0; year < daysPerMonth.Cheshvan.length; ++year) {
    let endOfCheshvan = Temporal.PlainDate.from({
      calendar: "hebrew",
      year,
      monthCode: "M02",
      day: 30,
    });
    assert.sameValue(endOfCheshvan.day, daysPerMonth.Cheshvan[year]);

    let endOfKislev = Temporal.PlainDate.from({
      calendar: "hebrew",
      year,
      monthCode: "M03",
      day: 30,
    });
    assert.sameValue(endOfKislev.day, daysPerMonth.Kislev[year]);
  }
}

