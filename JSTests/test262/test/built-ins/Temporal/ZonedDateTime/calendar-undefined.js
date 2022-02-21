// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: Calendar argument defaults to the built-in ISO 8601 calendar
features: [BigInt, Temporal]
---*/

const args = [957270896987654321n, new Temporal.TimeZone("UTC")];

Object.defineProperty(Temporal.Calendar, "from", {
  get() {
    throw new Test262Error("Should not get Calendar.from");
  },
});

const explicit = new Temporal.ZonedDateTime(...args, undefined);
assert.sameValue(explicit.calendar.toString(), "iso8601");

const implicit = new Temporal.ZonedDateTime(...args);
assert.sameValue(implicit.calendar.toString(), "iso8601");
