// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: Properties on an object passed to toZonedDateTime() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalTimeZone
  "has timeZone.timeZone",
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
// clear observable operations that occurred during the constructor call
actual.splice(0);

const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone");

const options = TemporalHelpers.propertyBagObserver(actual, { disambiguation: "compatible" }, "options");

instance.toZonedDateTime(timeZone, options);
assert.compareArray(actual, expected, "order of operations");
