// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar
description: Test for Temporal.Calendar subclassing.
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
}

const instance = new CustomCalendar("iso8601");
assert.sameValue(instance.toString(), "iso8601");
assert.sameValue(Object.getPrototypeOf(instance), CustomCalendar.prototype, "Instance of CustomCalendar");
assert(instance instanceof CustomCalendar, "Instance of CustomCalendar");
assert(instance instanceof Temporal.Calendar, "Instance of Temporal.Calendar");
