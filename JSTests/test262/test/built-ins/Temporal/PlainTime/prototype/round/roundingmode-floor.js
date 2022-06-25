// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.round
description: Tests calculations with roundingMode "floor".
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const plainTime = Temporal.PlainTime.from("13:46:23.123456789");

TemporalHelpers.assertPlainTime(
  plainTime.round({ smallestUnit: "hour", roundingMode: "floor" }),
  13, 0, 0, 0, 0, 0, "hour");

TemporalHelpers.assertPlainTime(
  plainTime.round({ smallestUnit: "minute", roundingMode: "floor" }),
  13, 46, 0, 0, 0, 0, "minute");

TemporalHelpers.assertPlainTime(
  plainTime.round({ smallestUnit: "second", roundingMode: "floor" }),
  13, 46, 23, 0, 0, 0, "second");

TemporalHelpers.assertPlainTime(
  plainTime.round({ smallestUnit: "millisecond", roundingMode: "floor" }),
  13, 46, 23, 123, 0, 0, "millisecond");

TemporalHelpers.assertPlainTime(
  plainTime.round({ smallestUnit: "microsecond", roundingMode: "floor" }),
  13, 46, 23, 123, 456, 0, "microsecond");

TemporalHelpers.assertPlainTime(
  plainTime.round({ smallestUnit: "nanosecond", roundingMode: "floor" }),
  13, 46, 23, 123, 456, 789, "nanosecond");
