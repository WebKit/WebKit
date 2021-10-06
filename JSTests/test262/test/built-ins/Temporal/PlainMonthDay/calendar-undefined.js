// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday
description: Calendar argument defaults to the built-in ISO 8601 calendar
features: [Temporal]
---*/

const args = [5, 2];

Object.defineProperty(Temporal.Calendar, "from", {
  get() {
    throw new Test262Error("Should not get Calendar.from");
  },
});

const dateExplicit = new Temporal.PlainMonthDay(...args, undefined);
assert.sameValue(dateExplicit.calendar.toString(), "iso8601");

const dateImplicit = new Temporal.PlainMonthDay(...args);
assert.sameValue(dateImplicit.calendar.toString(), "iso8601");
