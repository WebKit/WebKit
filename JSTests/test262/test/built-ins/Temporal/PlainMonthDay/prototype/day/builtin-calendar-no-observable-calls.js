// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.day
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dayOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "day");
Object.defineProperty(Temporal.Calendar.prototype, "day", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("day should not be looked up");
  },
});

const instance = new Temporal.PlainMonthDay(5, 2, "iso8601", 1972);
instance.day;

Object.defineProperty(Temporal.Calendar.prototype, "day", dayOriginal);
