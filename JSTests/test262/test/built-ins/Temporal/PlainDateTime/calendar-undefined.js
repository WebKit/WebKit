// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime
description: Calendar argument defaults to the built-in ISO 8601 calendar
features: [Temporal]
---*/

const dateTimeArgs = [2020, 12, 24, 12, 34, 56, 123, 456, 789];

Object.defineProperty(Temporal.Calendar, "from", {
  get() {
    throw new Test262Error("Should not get Calendar.from");
  },
});

const dateTimeExplicit = new Temporal.PlainDateTime(...dateTimeArgs, undefined);
assert.sameValue(dateTimeExplicit.calendar.toString(), "iso8601");

const dateTimeImplicit = new Temporal.PlainDateTime(...dateTimeArgs);
assert.sameValue(dateTimeImplicit.calendar.toString(), "iso8601");
