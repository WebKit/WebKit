// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Properties on an object passed to getInstantFor() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // GetTemporalCalendarWithISODefault
  "get fields.calendar",
  "has fields.calendar.calendar",
  // CalendarFields
  "get fields.calendar.fields",
  "call fields.calendar.fields",
  // PrepareTemporalFields
  "get fields.day",
  "get fields.day.valueOf",
  "call fields.day.valueOf",
  "get fields.hour",
  "get fields.hour.valueOf",
  "call fields.hour.valueOf",
  "get fields.microsecond",
  "get fields.microsecond.valueOf",
  "call fields.microsecond.valueOf",
  "get fields.millisecond",
  "get fields.millisecond.valueOf",
  "call fields.millisecond.valueOf",
  "get fields.minute",
  "get fields.minute.valueOf",
  "call fields.minute.valueOf",
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.nanosecond",
  "get fields.nanosecond.valueOf",
  "call fields.nanosecond.valueOf",
  "get fields.second",
  "get fields.second.valueOf",
  "call fields.second.valueOf",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  // InterpretTemporalDateTimeFields
  "get fields.calendar.dateFromFields",
  "call fields.calendar.dateFromFields",
  // ToTemporalDisambiguation
  "get options.disambiguation",
  "get options.disambiguation.toString",
  "call options.disambiguation.toString",
  // BuiltinTimeZoneGetInstantFor
  "get this.getPossibleInstantsFor",
  "call this.getPossibleInstantsFor",
];
const actual = [];

const instance = new Temporal.TimeZone("UTC");
TemporalHelpers.observeProperty(actual, instance, "getPossibleInstantsFor", function getPossibleInstantsFor(...args) {
  actual.push("call this.getPossibleInstantsFor");
  return Temporal.TimeZone.prototype.getPossibleInstantsFor.apply(instance, args);
}, "this");

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 5,
  monthCode: "M05",
  day: 2,
  hour: 12,
  minute: 34,
  second: 56,
  millisecond: 987,
  microsecond: 654,
  nanosecond: 321,
  calendar: TemporalHelpers.calendarObserver(actual, "fields.calendar"),
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, { disambiguation: "compatible" }, "options");

instance.getInstantFor(fields, options);
assert.compareArray(actual, expected, "order of operations");
