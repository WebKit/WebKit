// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Tests calculations with roundingMode "floor".
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = Temporal.PlainDate.from("2019-01-08");
const later = Temporal.PlainDate.from("2021-09-07");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "years", roundingMode: "floor" }),
  /* years = */ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, "years");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "years", roundingMode: "floor" }),
  /* years = */ -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, "years");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "months", roundingMode: "floor" }),
  0, /* months = */ 31, 0, 0, 0, 0, 0, 0, 0, 0, "months");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "months", roundingMode: "floor" }),
  0, /* months = */ -32, 0, 0, 0, 0, 0, 0, 0, 0, "months");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "weeks", roundingMode: "floor" }),
  0, 0, /* weeks = */ 139, 0, 0, 0, 0, 0, 0, 0, "weeks");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "weeks", roundingMode: "floor" }),
  0, 0, /* weeks = */ -139, 0, 0, 0, 0, 0, 0, 0, "weeks");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "days", roundingMode: "floor" }),
  0, 0, 0, /* days = */ 973, 0, 0, 0, 0, 0, 0, "days");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "days", roundingMode: "floor" }),
  0, 0, 0, /* days = */ -973, 0, 0, 0, 0, 0, 0, "days");
