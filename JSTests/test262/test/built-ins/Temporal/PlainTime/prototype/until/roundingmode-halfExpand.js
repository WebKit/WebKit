// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.until
description: Tests calculations with roundingMode "halfExpand".
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = Temporal.PlainTime.from("08:22:36.123456789");
const later = Temporal.PlainTime.from("12:39:40.987654321");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "hours", roundingMode: "halfExpand" }),
  0, 0, 0, 0, 4, 0, 0, 0, 0, 0, "hours");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "hours", roundingMode: "halfExpand" }),
  0, 0, 0, 0, -4, 0, 0, 0, 0, 0, "hours");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "minutes", roundingMode: "halfExpand" }),
  0, 0, 0, 0, 4, 17, 0, 0, 0, 0, "minutes");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "minutes", roundingMode: "halfExpand" }),
  0, 0, 0, 0, -4, -17, 0, 0, 0, 0, "minutes");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "seconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, 4, 17, 5, 0, 0, 0, "seconds");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "seconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, -4, -17, -5, 0, 0, 0, "seconds");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "milliseconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, 4, 17, 4, 864, 0, 0, "milliseconds");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "milliseconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, -4, -17, -4, -864, 0, 0, "milliseconds");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "microseconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, 4, 17, 4, 864, 198, 0, "microseconds");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "microseconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, -4, -17, -4, -864, -198, 0, "microseconds");

TemporalHelpers.assertDuration(
  earlier.until(later, { smallestUnit: "nanoseconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, 4, 17, 4, 864, 197, 532, "nanoseconds");
TemporalHelpers.assertDuration(
  later.until(earlier, { smallestUnit: "nanoseconds", roundingMode: "halfExpand" }),
  0, 0, 0, 0, -4, -17, -4, -864, -197, -532, "nanoseconds");
