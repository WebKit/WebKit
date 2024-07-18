// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Date arithmetic with largestUnit "week"
features: [Temporal]
includes: [temporalHelpers.js]
---*/

["week", "weeks"].forEach(function(largestUnit) {
  let opt = {largestUnit};

  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-16", opt),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "same day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-17", opt),
      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "one day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-23", opt),
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, "7 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-08-16", opt),
      0, 0, 4, 3, 0, 0, 0, 0, 0, 0, "4 weeks and 3 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-08-13", opt),
      0, 0, 4, 0, 0, 0, 0, 0, 0, 0, "4 weeks");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-09-16", opt),
      0, 0, 8, 6, 0, 0, 0, 0, 0, 0, "8 weeks and 6 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2022-07-16", opt),
      0, 0, 52, 1, 0, 0, 0, 0, 0, 0, "52 weeks and 1 day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2031-07-16", opt),
      0, 0, 521, 5, 0, 0, 0, 0, 0, 0, "521 weeks and 5 days");

  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-17").until("2021-07-16", opt),
      0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "negative one day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-23").until("2021-07-16", opt),
      0, 0, -1, 0, 0, 0, 0, 0, 0, 0, "negative 7 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-08-16").until("2021-07-16", opt),
      0, 0, -4, -3, 0, 0, 0, 0, 0, 0, "negative 4 weeks and 3 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-08-13").until("2021-07-16", opt),
      0, 0, -4, 0, 0, 0, 0, 0, 0, 0, "negative 4 weeks");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-09-16").until("2021-07-16", opt),
      0, 0, -8, -6, 0, 0, 0, 0, 0, 0, "negative 8 weeks and 6 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2022-07-16").until("2021-07-16", opt),
      0, 0, -52, -1, 0, 0, 0, 0, 0, 0, "negative 52 weeks and 1 day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2031-07-16").until("2021-07-16", opt),
      0, 0, -521, -5, 0, 0, 0, 0, 0, 0, "negative 521 weeks and 5 days");
});
