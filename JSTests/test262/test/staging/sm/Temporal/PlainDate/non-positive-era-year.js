// Copyright (C) 2025 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

const tests = [
  {
    calendar: "gregory",
    era: "ce",
    start: "0001-01-01",
  },
  {
    calendar: "gregory",
    era: "bce",
    inverse: true,
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
    era: "ce",
    start: "0001-01-01",
  },
  {
    calendar: "japanese",
    era: "bce",
    inverse: true,
    start: "0000-01-01",
  },

  {
    calendar: "buddhist",
    era: "be",
    start: "-000542-01-01",
  },

  {
    calendar: "coptic",
    era: "am",
    start: "0284-08-29",
  },

  {
    calendar: "ethioaa",
    era: "aa",
    start: "-005492-07-18",
  },

  {
    calendar: "ethiopic",
    era: "am",
    start: "0008-08-27",
  },
  {
    calendar: "ethiopic",
    era: "aa",
    start: "-005492-07-18",
  },

  {
    calendar: "hebrew",
    era: "am",
    start: "-003760-09-07",
  },

  {
    calendar: "indian",
    era: "shaka",
    start: "0079-03-23",
  },

  {
    calendar: "islamic-civil",
    era: "ah",
    start: "0622-07-20",
  },
  {
    calendar: "islamic-civil",
    era: "bh",
    inverse: true,
    start: "0622-01-01",
  },

  {
    calendar: "islamic-tbla",
    era: "ah",
    start: "0622-07-19",
  },
  {
    calendar: "islamic-tbla",
    era: "bh",
    inverse: true,
    start: "0622-01-01",
  },

  {
    calendar: "islamic-umalqura",
    era: "ah",
    start: "0622-07-20",
  },
  {
    calendar: "islamic-umalqura",
    era: "bh",
    inverse: true,
    start: "0622-01-01",
  },

  {
    calendar: "persian",
    era: "ap",
    start: "0622-03-22",
  },

  {
    calendar: "roc",
    era: "roc",
    start: "1912-01-01",
  },
  {
    calendar: "roc",
    era: "broc",
    inverse: true,
    start: "1911-01-01",
  },
];

for (let {calendar, era, start, inverse} of tests) {
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
    if (inverse) {
      years = -years;
    }

    let expected = eraStart.add({years}).with({monthCode, day});

    assert.sameValue(date.equals(expected), true, `${date} != ${expected} (${calendar} era ${era} year ${eraYear})`);
  }
}

