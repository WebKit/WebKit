// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: Tests calculations with roundingMode "floor".
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = Temporal.PlainYearMonth.from("2019-01");
const later = Temporal.PlainYearMonth.from("2021-09");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "years", roundingMode: "floor" }),
  /* years = */ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, "years");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "years", roundingMode: "floor" }),
  /* years = */ -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, "years");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "months", roundingMode: "floor" }),
  /* years = */ 2, /* months = */ 8, 0, 0, 0, 0, 0, 0, 0, 0, "months");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "months", roundingMode: "floor" }),
  /* years = */ -2, /* months = */ -8, 0, 0, 0, 0, 0, 0, 0, 0, "months");
