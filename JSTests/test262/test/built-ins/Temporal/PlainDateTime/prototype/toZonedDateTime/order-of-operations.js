// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: Properties on an object passed to toZonedDateTime() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalTimeZoneSlotValue
  "has timeZone.getOffsetNanosecondsFor",
  "has timeZone.getPossibleInstantsFor",
  "has timeZone.id",
  // ToTemporalDisambiguation
  "get options.disambiguation",
  "get options.disambiguation.toString",
  "call options.disambiguation.toString",
  // BuiltinTimeZoneGetInstantFor
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
];
const actual = [];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
const fallBackInstance = new Temporal.PlainDateTime(2000, 10, 29, 1, 30, 0, 0, 0, 0, calendar);
const springForwardInstance = new Temporal.PlainDateTime(2000, 4, 2, 2, 30, 0, 0, 0, 0, calendar);
// clear observable operations that occurred during the constructor calls
actual.splice(0);

const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor,
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor,
});

const options = TemporalHelpers.propertyBagObserver(actual, { disambiguation: "compatible" }, "options");

instance.toZonedDateTime(timeZone, options);
assert.compareArray(actual, expected, "order of operations at normal wall-clock time");
actual.splice(0); // clear

fallBackInstance.toZonedDateTime(timeZone, options);
assert.compareArray(actual, expected, "order of operations at repeated wall-clock time");
actual.splice(0); // clear

springForwardInstance.toZonedDateTime(timeZone, options);
assert.compareArray(actual, expected.concat([
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
]), "order of operations at skipped wall-clock time");
actual.splice(0); // clear

const rejectOptions = TemporalHelpers.propertyBagObserver(actual, { disambiguation: "reject" }, "options");
assert.throws(RangeError, () => springForwardInstance.toZonedDateTime(timeZone, rejectOptions));
assert.compareArray(actual, expected, "order of operations at skipped wall-clock time with disambiguation: reject");
actual.splice(0); // clear
