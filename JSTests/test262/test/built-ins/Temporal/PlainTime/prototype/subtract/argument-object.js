// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.subtract
description: Plain object arguments are supported.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainTime = new Temporal.PlainTime(15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ hours: 16 }),
  23, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ minutes: 45 }),
  14, 38, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ seconds: 45 }),
  15, 22, 45, 123, 456, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ milliseconds: 800 }),
  15, 23, 29, 323, 456, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ microseconds: 800 }),
  15, 23, 30, 122, 656, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ nanoseconds: 800 }),
  15, 23, 30, 123, 455, 989);
TemporalHelpers.assertPlainTime(Temporal.PlainTime.from("23:23:30.123456789").subtract({ hours: -16 }),
  15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(Temporal.PlainTime.from("14:38:30.123456789").subtract({ minutes: -45 }),
  15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(Temporal.PlainTime.from("15:22:45.123456789").subtract({ seconds: -45 }),
  15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(Temporal.PlainTime.from("15:23:29.323456789").subtract({ milliseconds: -800 }),
  15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(Temporal.PlainTime.from("15:23:30.122656789").subtract({ microseconds: -800 }),
  15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(Temporal.PlainTime.from("15:23:30.123455989").subtract({ nanoseconds: -800 }),
  15, 23, 30, 123, 456, 789);
TemporalHelpers.assertPlainTime(plainTime.subtract({ minute: 1, hours: 1 }),
  14, 23, 30, 123, 456, 789);
