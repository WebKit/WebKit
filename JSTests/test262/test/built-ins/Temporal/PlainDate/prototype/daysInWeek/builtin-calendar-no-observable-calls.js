// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.daysinweek
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const daysInWeekOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "daysInWeek");
Object.defineProperty(Temporal.Calendar.prototype, "daysInWeek", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("daysInWeek should not be looked up");
  },
});

const instance = new Temporal.PlainDate(2000, 5, 2, "iso8601");
instance.daysInWeek;

Object.defineProperty(Temporal.Calendar.prototype, "daysInWeek", daysInWeekOriginal);
