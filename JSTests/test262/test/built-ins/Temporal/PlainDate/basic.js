// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate
description: Basic tests for the PlainDate constructor.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

Object.defineProperty(Temporal.Calendar, "from", {
  get() {
    throw new Test262Error("Should not get Calendar.from");
  },
});

const calendar = new Temporal.Calendar("iso8601");
const plainDateWithObject = new Temporal.PlainDate(2020, 12, 24, calendar);
TemporalHelpers.assertPlainDate(plainDateWithObject, 2020, 12, "M12", 24, "with object");
assert.sameValue(plainDateWithObject.getCalendar(), calendar);

const plainDateWithString = new Temporal.PlainDate(2020, 12, 24, "iso8601");
TemporalHelpers.assertPlainDate(plainDateWithString, 2020, 12, "M12", 24, "with string");
assert.sameValue(plainDateWithString.getISOFields().calendar, "iso8601", "calendar slot should store a string");
assert.notSameValue(plainDateWithString.getCalendar(), calendar);
