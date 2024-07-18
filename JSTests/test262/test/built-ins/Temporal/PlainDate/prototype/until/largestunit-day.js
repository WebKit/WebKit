// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Date arithmetic with largestUnit "day"
features: [Temporal]
includes: [temporalHelpers.js]
---*/

["day", "days"].forEach(function(largestUnit) {
  let opt = {largestUnit};
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-16", opt),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "same day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-07-17", opt),
      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "one day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-08-17", opt),
      0, 0, 0, 32, 0, 0, 0, 0, 0, 0, "32 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2021-09-16", opt),
      0, 0, 0, 62, 0, 0, 0, 0, 0, 0, "62 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2022-07-16", opt),
      0, 0, 0, 365, 0, 0, 0, 0, 0, 0, "365 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-16").until("2031-07-16", opt),
      0, 0, 0, 3652, 0, 0, 0, 0, 0, 0, "3652 days");

  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-07-17").until("2021-07-16", opt),
      0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "negative one day");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-08-17").until("2021-07-16", opt),
      0, 0, 0, -32, 0, 0, 0, 0, 0, 0, "negative 32 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2021-09-16").until("2021-07-16", opt),
      0, 0, 0, -62, 0, 0, 0, 0, 0, 0, "negative 62 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2022-07-16").until("2021-07-16", opt),
      0, 0, 0, -365, 0, 0, 0, 0, 0, 0, "negative 365 days");
  TemporalHelpers.assertDuration(
      Temporal.PlainDate.from("2031-07-16").until("2021-07-16", opt),
      0, 0, 0, -3652, 0, 0, 0, 0, 0, 0, "negative 3652 days");
});
