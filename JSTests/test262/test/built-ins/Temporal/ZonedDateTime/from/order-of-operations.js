// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Properties on objects passed to from() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get item.calendar",
  "has item.calendar.calendar",
  "get item.calendar.fields",
  "call item.calendar.fields",
  // PrepareTemporalFields
  "get item.day",
  "get item.day.valueOf",
  "call item.day.valueOf",
  "get item.hour",
  "get item.hour.valueOf",
  "call item.hour.valueOf",
  "get item.microsecond",
  "get item.microsecond.valueOf",
  "call item.microsecond.valueOf",
  "get item.millisecond",
  "get item.millisecond.valueOf",
  "call item.millisecond.valueOf",
  "get item.minute",
  "get item.minute.valueOf",
  "call item.minute.valueOf",
  "get item.month",
  "get item.month.valueOf",
  "call item.month.valueOf",
  "get item.monthCode",
  "get item.monthCode.toString",
  "call item.monthCode.toString",
  "get item.nanosecond",
  "get item.nanosecond.valueOf",
  "call item.nanosecond.valueOf",
  "get item.offset",
  "get item.offset.toString",
  "call item.offset.toString",
  "get item.second",
  "get item.second.valueOf",
  "call item.second.valueOf",
  "get item.timeZone",
  "get item.year",
  "get item.year.valueOf",
  "call item.year.valueOf",
  "has item.timeZone.timeZone",
  // InterpretTemporalDateTimeFields
  "get options.disambiguation",
  "get options.disambiguation.toString",
  "call options.disambiguation.toString",
  "get options.offset",
  "get options.offset.toString",
  "call options.offset.toString",
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  "get item.calendar.dateFromFields",
  "call item.calendar.dateFromFields",
  // inside calendar.dateFromFields
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  // InterpretISODateTimeOffset
  "get item.timeZone.getPossibleInstantsFor",
  "call item.timeZone.getPossibleInstantsFor",
  "get item.timeZone.getOffsetNanosecondsFor",
  "call item.timeZone.getOffsetNanosecondsFor",
];
const actual = [];

const from = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  day: 2,
  hour: 6,
  minute: 54,
  second: 32,
  millisecond: 987,
  microsecond: 654,
  nanosecond: 321,
  offset: "+00:00",
  calendar: TemporalHelpers.calendarObserver(actual, "item.calendar"),
  timeZone: TemporalHelpers.timeZoneObserver(actual, "item.timeZone"),
}, "item");

function createOptionsObserver({ overflow = "constrain", disambiguation = "compatible", offset = "reject" } = {}) {
  return TemporalHelpers.propertyBagObserver(actual, {
    overflow,
    disambiguation,
    offset,
  }, "options");
}

Temporal.ZonedDateTime.from(from, createOptionsObserver());
assert.compareArray(actual, expected, "order of operations");
