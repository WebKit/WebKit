// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Date arithmetic with largestUnit "year"
features: [Temporal]
includes: [temporalHelpers.js]
---*/

["year", "years"].forEach(function(largestUnit) {
  let opt = {largestUnit};
 TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-16", opt),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "same day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-17", opt),
      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "one day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-23", opt),
      0, 0, 0, 7, 0, 0, 0, 0, 0, 0, "7 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-08-16", opt),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, "1 month in same year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2020-12-16").until("2021-01-16", opt),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, "1 month in different year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-01-05").until("2021-02-05", opt),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, "1 month in same year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-01-07").until("2021-03-07", opt),
      0, 2, 0, 0, 0, 0, 0, 0, 0, 0, "2 month in same year across Feb 28");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-08-17", opt),
      0, 1, 0, 1, 0, 0, 0, 0, 0, 0, "1 month and 1 day in a month with 31 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-08-13", opt),
      0, 0, 0, 28, 0, 0, 0, 0, 0, 0, "28 days roll across a month which has 31 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-09-16", opt),
      0, 2, 0, 0, 0, 0, 0, 0, 0, 0, "2 months with both months which have 31 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2022-07-16", opt),
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1 year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2031-07-16", opt),
      10, 0, 0, 0, 0, 0, 0, 0, 0, 0, "10 years");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2022-07-19", opt),
      1, 0, 0, 3, 0, 0, 0, 0, 0, 0, "1 year and 3 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2022-09-19", opt),
      1, 2, 0, 3, 0, 0, 0, 0, 0, 0, "1 year 2 months and 3 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2031-12-16", opt),
      10, 5, 0, 0, 0, 0, 0, 0, 0, 0, "10 years and 5 months");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1997-12-16").until("2021-07-16", opt),
      23, 7, 0, 0, 0, 0, 0, 0, 0, 0, "23 years and 7 months");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1997-07-16").until("2021-07-16", opt),
      24, 0, 0, 0, 0, 0, 0, 0, 0, 0, "24 years");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1997-07-16").until("2021-07-15", opt),
      23, 11, 0, 29, 0, 0, 0, 0, 0, 0, "23 years, 11 months and 29 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1997-06-16").until("2021-06-15", opt),
      23, 11, 0, 30, 0, 0, 0, 0, 0, 0, "23 years, 11 months and 30 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1960-02-16").until("2020-03-16", opt),
      60, 1, 0, 0, 0, 0, 0, 0, 0, 0, "60 years, 1 month");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1960-02-16").until("2021-03-15", opt),
      61, 0, 0, 27, 0, 0, 0, 0, 0, 0, "61 years, 27 days in non leap year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1960-02-16").until("2020-03-15", opt),
      60, 0, 0, 28, 0, 0, 0, 0, 0, 0, "60 years, 28 days in leap year");

  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-03-30").until("2021-07-16", opt),
      0, 3, 0, 16, 0, 0, 0, 0, 0, 0, "3 months and 16 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2020-03-30").until("2021-07-16", opt),
      1, 3, 0, 16, 0, 0, 0, 0, 0, 0, "1 year, 3 months and 16 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1960-03-30").until("2021-07-16", opt),
      61, 3, 0, 16, 0, 0, 0, 0, 0, 0, "61 years, 3 months and 16 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2019-12-30").until("2021-07-16", opt),
      1, 6, 0, 16, 0, 0, 0, 0, 0, 0, "1 year, 6 months and 16 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2020-12-30").until("2021-07-16", opt),
      0, 6, 0, 16, 0, 0, 0, 0, 0, 0, "6 months and 16 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("1997-12-30").until("2021-07-16", opt),
      23, 6, 0, 16, 0, 0, 0, 0, 0, 0, "23 years, 6 months and 16 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("0001-12-25").until("2021-07-16", opt),
      2019, 6, 0, 21, 0, 0, 0, 0, 0, 0, "2019 years, 6 months and 21 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2019-12-30").until("2021-03-05", opt),
      1, 2, 0, 5, 0, 0, 0, 0, 0, 0, "1 year, 2 months and 5 days");

  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-17").until("2021-07-16", opt),
      0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "negative one day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-23").until("2021-07-16", opt),
      0, 0, 0, -7, 0, 0, 0, 0, 0, 0, "negative 7 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-08-16").until("2021-07-16", opt),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 month in same year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-01-16").until("2020-12-16", opt),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 month in different year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-02-05").until("2021-01-05", opt),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 month in same year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-03-07").until("2021-01-07", opt),
      0, -2, 0, 0, 0, 0, 0, 0, 0, 0, "negative 2 month in same year across Feb 28");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-08-17").until("2021-07-16", opt),
      0, -1, 0, -1, 0, 0, 0, 0, 0, 0, "negative 1 month and 1 day in a month with 31 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-08-13").until("2021-07-16", opt),
      0, 0, 0, -28, 0, 0, 0, 0, 0, 0, "negative 28 days roll across a month which has 31 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-09-16").until("2021-07-16", opt),
      0, -2, 0, 0, 0, 0, 0, 0, 0, 0, "negative 2 months with both months which have 31 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2022-07-16").until("2021-07-16", opt),
      -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2031-07-16").until("2021-07-16", opt),
      -10, 0, 0, 0, 0, 0, 0, 0, 0, 0, "negative 10 years");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2022-07-19").until("2021-07-16", opt),
      -1, 0, 0, -3, 0, 0, 0, 0, 0, 0, "negative 1 year and 3 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2022-09-19").until("2021-07-16", opt),
      -1, -2, 0, -3, 0, 0, 0, 0, 0, 0, "negative 1 year 2 months and 3 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2031-12-16").until("2021-07-16", opt),
      -10, -5, 0, 0, 0, 0, 0, 0, 0, 0, "negative 10 years and 5 months");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("1997-12-16", opt),
      -23, -7, 0, 0, 0, 0, 0, 0, 0, 0, "negative 23 years and 7 months");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("1997-07-16", opt),
      -24, 0, 0, 0, 0, 0, 0, 0, 0, 0, "negative 24 years");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-15").until("1997-07-16", opt),
      -23, -11, 0, -30, 0, 0, 0, 0, 0, 0, "negative 23 years, 11 months and 30 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-06-15").until("1997-06-16", opt),
      -23, -11, 0, -29, 0, 0, 0, 0, 0, 0, "negative 23 years, 11 months and 29 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2020-03-16").until("1960-02-16", opt),
      -60, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 60 years, 1 month");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-03-15").until("1960-02-16", opt),
      -61, 0, 0, -28, 0, 0, 0, 0, 0, 0, "negative 61 years, 28 days in non leap year");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2020-03-15").until("1960-02-16", opt),
      -60, 0, 0, -28, 0, 0, 0, 0, 0, 0, "negative 60 years, 28 days in leap year");

  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-03-30", opt),
      0, -3, 0, -17, 0, 0, 0, 0, 0, 0, "negative 3 months and 17 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2020-03-30", opt),
      -1, -3, 0, -17, 0, 0, 0, 0, 0, 0, "negative 1 year, 3 months and 17 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("1960-03-30", opt),
      -61, -3, 0, -17, 0, 0, 0, 0, 0, 0, "negative 61 years, 3 months and 17 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2019-12-30", opt),
      -1, -6, 0, -17, 0, 0, 0, 0, 0, 0, "negative 1 year, 6 months and 17 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2020-12-30", opt),
      0, -6, 0, -17, 0, 0, 0, 0, 0, 0, "negative 6 months and 17 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("1997-12-30", opt),
      -23, -6, 0, -17, 0, 0, 0, 0, 0, 0, "negative 23 years, 6 months and 17 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("0001-12-25", opt),
      -2019, -6, 0, -22, 0, 0, 0, 0, 0, 0, "negative 2019 years, 6 months and 22 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-03-05").until("2019-12-30", opt),
      -1, -2, 0, -6, 0, 0, 0, 0, 0, 0, "negative 1 year, 2 months and 6 days");
});
