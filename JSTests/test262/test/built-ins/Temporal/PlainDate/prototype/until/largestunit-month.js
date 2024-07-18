// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Date arithmetic with largestUnit "month"
features: [Temporal]
includes: [temporalHelpers.js]
---*/

["month", "months"].forEach(function(largestUnit) {
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
      0, 12, 0, 0, 0, 0, 0, 0, 0, 0, "12 months");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2031-07-16", opt),
      0, 120, 0, 0, 0, 0, 0, 0, 0, 0, "120 months");

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
      0, -12, 0, 0, 0, 0, 0, 0, 0, 0, "negative 12 months");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2031-07-16").until("2021-07-16", opt),
      0, -120, 0, 0, 0, 0, 0, 0, 0, 0, "negative 120 months");
});
