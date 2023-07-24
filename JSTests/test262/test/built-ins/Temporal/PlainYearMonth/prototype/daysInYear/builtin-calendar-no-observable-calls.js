// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.daysinyear
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const daysInYearOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "daysInYear");
Object.defineProperty(Temporal.Calendar.prototype, "daysInYear", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("daysInYear should not be looked up");
  },
});

const instance = new Temporal.PlainYearMonth(2000, 5, "iso8601", 1);
instance.daysInYear;

Object.defineProperty(Temporal.Calendar.prototype, "daysInYear", daysInYearOriginal);
