// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.dayofweek
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dayOfWeekOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "dayOfWeek");
Object.defineProperty(Temporal.Calendar.prototype, "dayOfWeek", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("dayOfWeek should not be looked up");
  },
});

const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, "iso8601");
instance.dayOfWeek;

Object.defineProperty(Temporal.Calendar.prototype, "dayOfWeek", dayOfWeekOriginal);
