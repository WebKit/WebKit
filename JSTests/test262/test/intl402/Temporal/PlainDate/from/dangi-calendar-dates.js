// Copyright 2025 Google Inc, Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: Test `from` with Dangi calendar
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const calendar = "dangi";

const cases = {
  year2000: {
    year: 1999,
    month: 11,
    monthCode: "M11",
    day: 25
  },
  year1900: {
    year: 1899,
    month: 12,
    monthCode: "M12",
    day: 1
  },
  year2050: {
    year: 2049,
    month: 12,
    monthCode: "M12",
    day: 8
  }
};
const dates = {
  year2000: Temporal.PlainDate.from("2000-01-01"),
  year1900: Temporal.PlainDate.from("1900-01-01"),
  year2050: Temporal.PlainDate.from("2050-01-01")
};
for (var [name, result] of Object.entries(cases)) {
  const date = dates[name];
  const inCal = date.withCalendar(calendar);

  TemporalHelpers.assertPlainDate(inCal, result.year, result.month, result.monthCode, result.day, name, result.era, result.eraYear);

  const dateRoundtrip2 = Temporal.PlainDate.from({
    calendar,
    year: result.year,
    day: result.day,
    monthCode: result.monthCode
  });
  TemporalHelpers.assertPlainDate(dateRoundtrip2, inCal.year, inCal.month, inCal.monthCode, inCal.day, name, inCal.era, inCal.eraYear);

  const dateRoundtrip3 = Temporal.PlainDate.from({
    calendar,
    year: result.year,
    day: result.day,
    month: result.month
  });
  TemporalHelpers.assertPlainDate(dateRoundtrip3, inCal.year, inCal.month, inCal.monthCode, inCal.day, name, inCal.era, inCal.eraYear);

  const dateRoundtrip4 = Temporal.PlainDate.from({
    calendar,
    year: result.year,
    day: result.day,
    monthCode: result.monthCode
  });
  TemporalHelpers.assertPlainDate(dateRoundtrip4, inCal.year, inCal.month, inCal.monthCode, inCal.day, name, inCal.era, inCal.eraYear);

  assert.throws(RangeError, () => Temporal.PlainDate.from({
    calendar,
    day: result.day,
    month: result.month === 1 ? 2 : result.month - 1,
    monthCode: result.monthCode,
    year: result.year
  }));
}

