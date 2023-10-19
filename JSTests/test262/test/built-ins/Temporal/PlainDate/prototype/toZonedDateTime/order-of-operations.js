// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: User code calls happen in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get item.timeZone",
  "has item.timeZone.getOffsetNanosecondsFor",
  "has item.timeZone.getPossibleInstantsFor",
  "has item.timeZone.id",
  "get item.plainTime",
  // ToTemporalTime
  "get item.plainTime.hour",
  "get item.plainTime.hour.valueOf",
  "call item.plainTime.hour.valueOf",
  "get item.plainTime.microsecond",
  "get item.plainTime.microsecond.valueOf",
  "call item.plainTime.microsecond.valueOf",
  "get item.plainTime.millisecond",
  "get item.plainTime.millisecond.valueOf",
  "call item.plainTime.millisecond.valueOf",
  "get item.plainTime.minute",
  "get item.plainTime.minute.valueOf",
  "call item.plainTime.minute.valueOf",
  "get item.plainTime.nanosecond",
  "get item.plainTime.nanosecond.valueOf",
  "call item.plainTime.nanosecond.valueOf",
  "get item.plainTime.second",
  "get item.plainTime.second.valueOf",
  "call item.plainTime.second.valueOf",
  // GetInstantFor
  "get item.timeZone.getPossibleInstantsFor",
  "call item.timeZone.getPossibleInstantsFor",
];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDate(2000, 1, 1, calendar);
const springForwardInstance = new Temporal.PlainDate(2000, 4, 2, calendar);
const fallBackInstance = new Temporal.PlainDate(2000, 10, 29, calendar);
actual.splice(0); // clear calendar calls that happened in constructors

const plainTime = TemporalHelpers.propertyBagObserver(actual, {
  hour: 2,
  minute: 30,
  second: 0,
  millisecond: 0,
  microsecond: 0,
  nanosecond: 0,
}, "item.plainTime");
const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const timeZone = TemporalHelpers.timeZoneObserver(actual, "item.timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor,
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor,
});
const item = TemporalHelpers.propertyBagObserver(actual, {
  plainTime,
  timeZone,
}, "item");

instance.toZonedDateTime(item);
assert.compareArray(actual, expected, "order of operations at normal wall-clock time");
actual.splice(0); // clear

const plainTime130 = TemporalHelpers.propertyBagObserver(actual, {
  hour: 1,
  minute: 30,
  second: 0,
  millisecond: 0,
  microsecond: 0,
  nanosecond: 0,
}, "item.plainTime");
const item130 = TemporalHelpers.propertyBagObserver(actual, {
  plainTime: plainTime130,
  timeZone,
}, "item");

fallBackInstance.toZonedDateTime(item130);
assert.compareArray(actual, expected, "order of operations at repeated wall-clock time");
actual.splice(0); // clear

springForwardInstance.toZonedDateTime(item);
assert.compareArray(actual, expected.concat([
  "get item.timeZone.getOffsetNanosecondsFor",
  "call item.timeZone.getOffsetNanosecondsFor",
  "call item.timeZone.getOffsetNanosecondsFor",
  "get item.timeZone.getPossibleInstantsFor",
  "call item.timeZone.getPossibleInstantsFor",
]), "order of operations at skipped wall-clock time");
actual.splice(0); // clear
