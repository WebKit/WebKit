// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: Tests calculations with roundingMode "floor".
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = Temporal.PlainDate.from("2019-01-08");
const later = Temporal.PlainDate.from("2021-09-07");

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "years", roundingMode: "floor" }),
  /* years = */ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, "years");
TemporalHelpers.assertDuration(
  earlier.since(later, { smallestUnit: "years", roundingMode: "floor" }),
  /* years = */ -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, "years");

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "months", roundingMode: "floor" }),
  0, /* months = */ 31, 0, 0, 0, 0, 0, 0, 0, 0, "months");
TemporalHelpers.assertDuration(
  earlier.since(later, { smallestUnit: "months", roundingMode: "floor" }),
  0, /* months = */ -32, 0, 0, 0, 0, 0, 0, 0, 0, "months");

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "weeks", roundingMode: "floor" }),
  0, 0, /* weeks = */ 139, 0, 0, 0, 0, 0, 0, 0, "weeks");
TemporalHelpers.assertDuration(
  earlier.since(later, { smallestUnit: "weeks", roundingMode: "floor" }),
  0, 0, /* weeks = */ -139, 0, 0, 0, 0, 0, 0, 0, "weeks");

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "days", roundingMode: "floor" }),
  0, 0, 0, /* days = */ 973, 0, 0, 0, 0, 0, 0, "days");
TemporalHelpers.assertDuration(
  earlier.since(later, { smallestUnit: "days", roundingMode: "floor" }),
  0, 0, 0, /* days = */ -973, 0, 0, 0, 0, 0, 0, "days");
