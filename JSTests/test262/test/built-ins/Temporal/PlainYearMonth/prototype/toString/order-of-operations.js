// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.tostring
description: Properties on an object passed to toString() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get options.calendarName",
  "get options.calendarName.toString",
  "call options.calendarName.toString",
];
const actual = [];

const instance = new Temporal.PlainYearMonth(2000, 5);

const options = TemporalHelpers.propertyBagObserver(actual, {
  calendarName: "auto",
}, "options");

instance.toString(options);
assert.compareArray(actual, expected, "order of operations");
