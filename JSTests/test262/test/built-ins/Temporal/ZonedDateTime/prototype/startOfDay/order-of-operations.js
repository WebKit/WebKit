// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.startofday
description: User code calls happen in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // GetPlainDateTimeFor
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // GetInstantFor on preceding midnight
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
];
const actual = [];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(
  1_000_000_000_000_000_000n,
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone"),
  calendar,
);

const fallBackTimeZone = TemporalHelpers.oneShiftTimeZone(Temporal.Instant.fromEpochSeconds(1800), -3600_000_000_000);
const fallBackInstance = new Temporal.ZonedDateTime(
  0n,
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
    getOffsetNanosecondsFor: fallBackTimeZone.getOffsetNanosecondsFor.bind(fallBackTimeZone),
    getPossibleInstantsFor: fallBackTimeZone.getPossibleInstantsFor.bind(fallBackTimeZone),
  }),
  calendar,
);
const springForwardTimeZone = TemporalHelpers.oneShiftTimeZone(Temporal.Instant.fromEpochSeconds(-1800), 3600_000_000_000);
const springForwardInstance = new Temporal.ZonedDateTime(
  0n,
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
    getOffsetNanosecondsFor: springForwardTimeZone.getOffsetNanosecondsFor.bind(springForwardTimeZone),
    getPossibleInstantsFor: springForwardTimeZone.getPossibleInstantsFor.bind(springForwardTimeZone),
  }),
  calendar,
);
// clear any observable operations that happen due to time zone or calendar
// calls in the constructors
actual.splice(0);

instance.startOfDay();
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

fallBackInstance.startOfDay();
assert.compareArray(actual, expected, "order of operations with preceding midnight at repeated wall-clock time");
actual.splice(0); // clear

springForwardInstance.startOfDay();
assert.compareArray(actual, expected.concat([
  // DisambiguatePossibleInstants
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
]), "order of operations with preceding midnight at skipped wall-clock time");
actual.splice(0); // clear
