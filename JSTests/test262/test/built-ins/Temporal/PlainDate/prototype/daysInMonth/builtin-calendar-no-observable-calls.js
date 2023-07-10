// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.daysinmonth
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const daysInMonthOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "daysInMonth");
Object.defineProperty(Temporal.Calendar.prototype, "daysInMonth", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("daysInMonth should not be looked up");
  },
});

const instance = new Temporal.PlainDate(2000, 5, 2, "iso8601");
instance.daysInMonth;

Object.defineProperty(Temporal.Calendar.prototype, "daysInMonth", daysInMonthOriginal);
