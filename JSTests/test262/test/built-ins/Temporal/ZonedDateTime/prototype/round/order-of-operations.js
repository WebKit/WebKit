// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: Properties on objects passed to round() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get options.roundingIncrement",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
  // GetPlainDateTimeFor on receiver's instant
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // GetInstantFor on preceding midnight
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // AddDaysToZonedDateTime
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // InterpretISODateTimeOffset
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
];
const actual = [];

const options = TemporalHelpers.propertyBagObserver(actual, {
  smallestUnit: "nanoseconds",
  roundingMode: "halfExpand",
  roundingIncrement: 2,
}, "options");

const nextHourOptions = TemporalHelpers.propertyBagObserver(actual, {
  smallestUnit: "hour",
  roundingMode: "ceil",
  roundingIncrement: 1,
}, "options");

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(
  988786472_987_654_321n,  /* 2001-05-02T06:54:32.987654321Z */
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone"),
  calendar,
);

const fallBackTimeZone = TemporalHelpers.oneShiftTimeZone(Temporal.Instant.fromEpochSeconds(1800), -3600_000_000_000);
const fallBackTimeZoneObserver = TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
  getOffsetNanosecondsFor: fallBackTimeZone.getOffsetNanosecondsFor.bind(fallBackTimeZone),
  getPossibleInstantsFor: fallBackTimeZone.getPossibleInstantsFor.bind(fallBackTimeZone),
});
const fallBackInstance = new Temporal.ZonedDateTime(0n, fallBackTimeZoneObserver, calendar);
const beforeFallBackInstance = new Temporal.ZonedDateTime(-3599_000_000_000n, fallBackTimeZoneObserver, calendar);

const springForwardTimeZone = TemporalHelpers.oneShiftTimeZone(Temporal.Instant.fromEpochSeconds(-1800), 3600_000_000_000);
const springForwardTimeZoneObserver = TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
  getOffsetNanosecondsFor: springForwardTimeZone.getOffsetNanosecondsFor.bind(springForwardTimeZone),
  getPossibleInstantsFor: springForwardTimeZone.getPossibleInstantsFor.bind(springForwardTimeZone),
});
const springForwardInstance = new Temporal.ZonedDateTime(0n, springForwardTimeZoneObserver, calendar);
const beforeSpringForwardInstance = new Temporal.ZonedDateTime(-3599_000_000_000n, springForwardTimeZoneObserver, calendar);
// clear any observable operations that happen due to time zone or calendar
// calls in the constructors
actual.splice(0);

instance.round(options);
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

fallBackInstance.round(options);
assert.compareArray(actual, expected, "order of operations with preceding midnight at repeated wall-clock time");
actual.splice(0); // clear

beforeFallBackInstance.round(nextHourOptions);
assert.compareArray(actual, expected, "order of operations with rounding result at repeated wall-clock time");
actual.splice(0); // clear

const expectedSkippedDateTime = [
  "get options.roundingIncrement",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
  // GetPlainDateTimeFor on receiver's instant
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // GetInstantFor on preceding midnight
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // AddZonedDateTime
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // InterpretISODateTimeOffset
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
];

springForwardInstance.round(options);
assert.compareArray(actual, expectedSkippedDateTime, "order of operations with preceding midnight at skipped wall-clock time");
actual.splice(0); // clear

const expectedSkippedResult = [
  "get options.roundingIncrement",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
  // GetPlainDateTimeFor on receiver's instant
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // GetInstantFor on preceding midnight
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // AddDaysToZonedDateTime
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // InterpretISODateTimeOffset
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
];

beforeSpringForwardInstance.round(nextHourOptions);
assert.compareArray(
  actual,
  expectedSkippedResult,
  "order of operations with following midnight and rounding result at skipped wall-clock time"
);
actual.splice(0); // clear
