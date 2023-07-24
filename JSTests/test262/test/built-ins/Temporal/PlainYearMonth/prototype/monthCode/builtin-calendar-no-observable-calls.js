// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.monthcode
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const monthCodeOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "monthCode");
Object.defineProperty(Temporal.Calendar.prototype, "monthCode", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("monthCode should not be looked up");
  },
});

const instance = new Temporal.PlainYearMonth(2000, 5, "iso8601", 1);
instance.monthCode;

Object.defineProperty(Temporal.Calendar.prototype, "monthCode", monthCodeOriginal);
