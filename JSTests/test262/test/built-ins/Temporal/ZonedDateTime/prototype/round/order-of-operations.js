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
  // AddZonedDateTime
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
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

const instance = new Temporal.ZonedDateTime(
  988786472_987_654_321n,  /* 2001-05-02T06:54:32.987654321Z */
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone"),
  TemporalHelpers.calendarObserver(actual, "this.calendar"),
);
// clear any observable operations that happen due to time zone or calendar
// calls on the constructor
actual.splice(0);

instance.round(options);
assert.compareArray(actual, expected, "order of operations");
