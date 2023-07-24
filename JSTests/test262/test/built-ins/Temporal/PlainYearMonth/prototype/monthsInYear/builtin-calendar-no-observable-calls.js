// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.monthsinyear
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const monthsInYearOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "monthsInYear");
Object.defineProperty(Temporal.Calendar.prototype, "monthsInYear", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("monthsInYear should not be looked up");
  },
});

const instance = new Temporal.PlainYearMonth(2000, 5, "iso8601", 1);
instance.monthsInYear;

Object.defineProperty(Temporal.Calendar.prototype, "monthsInYear", monthsInYearOriginal);
