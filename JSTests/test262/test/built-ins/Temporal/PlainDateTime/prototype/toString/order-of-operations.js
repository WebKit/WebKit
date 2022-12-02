// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tostring
description: Properties on objects passed to toString() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get options.calendarName",
  "get options.calendarName.toString",
  "call options.calendarName.toString",
  "get options.fractionalSecondDigits",
  "get options.fractionalSecondDigits.toString",
  "call options.fractionalSecondDigits.toString",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
  "get this.calendar[Symbol.toPrimitive]",
  "get this.calendar.toString",
  "call this.calendar.toString",
];
const actual = [];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDateTime(1990, 11, 3, 15, 54, 37, 123, 456, 789, calendar);
// clear observable operations that occurred during the constructor call
actual.splice(0);

instance.toString(
  TemporalHelpers.propertyBagObserver(actual, {
    fractionalSecondDigits: "auto",
    roundingMode: "halfExpand",
    smallestUnit: "millisecond",
    calendarName: "auto",
  }, "options"),
);
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

// Same as above but without options.smallestUnit.toString
const expectedForFractionalSecondDigits = [
  "get options.calendarName",
  "get options.calendarName.toString",
  "call options.calendarName.toString",
  "get options.fractionalSecondDigits",
  "get options.fractionalSecondDigits.toString",
  "call options.fractionalSecondDigits.toString",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get this.calendar[Symbol.toPrimitive]",
  "get this.calendar.toString",
  "call this.calendar.toString",
];

instance.toString(
  TemporalHelpers.propertyBagObserver(actual, {
    fractionalSecondDigits: "auto",
    roundingMode: "halfExpand",
    smallestUnit: undefined,
    calendarName: "auto",
  }, "options"),
);
assert.compareArray(actual, expectedForFractionalSecondDigits, "order of operations with smallestUnit undefined");
