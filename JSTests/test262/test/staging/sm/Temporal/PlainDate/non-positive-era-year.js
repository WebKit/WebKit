// Copyright (C) 2025 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
features:
  - Temporal
description: |
  pending
esid: pending
---*/

const tests = [
  {
    calendar: "gregory",
    era: "gregory",
    start: "0001-01-01",
  },
  {
    calendar: "gregory",
    era: "gregory-inverse",
    start: "0000-01-01",
  },

  {
    calendar: "japanese",
    era: "reiwa",
    start: "2019-05-01",
  },
  {
    calendar: "japanese",
    era: "heisei",
    start: "1989-01-08",
  },
  {
    calendar: "japanese",
    era: "showa",
    start: "1926-12-25",
  },
  {
    calendar: "japanese",
    era: "taisho",
    start: "1912-07-30",
  },
  {
    calendar: "japanese",
    era: "meiji",
    // Start date is actually October 23, 1868.
    //
    // https://github.com/unicode-org/icu4x/issues/4892
    // https://unicode-org.atlassian.net/browse/CLDR-11375
    start: "1868-09-08",
  },
  {
    calendar: "japanese",
    era: "japanese",
    start: "0001-01-01",
  },
  {
    calendar: "japanese",
    era: "japanese-inverse",
    start: "0000-01-01",
  },

  {
    calendar: "coptic",
    era: "coptic",
    start: "0284-08-29",
  },
  {
    calendar: "coptic",
    era: "coptic-inverse",
    start: "0283-08-30",
  },

  {
    calendar: "ethiopic",
    era: "ethiopic",
    start: "0008-08-27",
  },

  {
    calendar: "roc",
    era: "roc",
    start: "1912-01-01",
  },
  {
    calendar: "roc",
    era: "roc-inverse",
    start: "1911-01-01",
  },
];

for (let {calendar, era, start} of tests) {
  let eraStart = Temporal.PlainDate.from(start).withCalendar(calendar);

  let monthCode = "M01";
  let day = 1;

  for (let eraYear of [1, 0, -1]) {
    let date = Temporal.PlainDate.from({
      calendar,
      era,
      eraYear,
      monthCode,
      day,
    });

    let years = eraYear - 1;
    if (era.endsWith("-inverse")) {
      years = -years;
    }

    let expected = eraStart.add({years}).with({monthCode, day});

    assert.sameValue(date.equals(expected), true, `${date} != ${expected}`);
  }
}

