// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dateUntilOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "dateUntil");
Object.defineProperty(Temporal.Calendar.prototype, "dateUntil", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("dateUntil should not be looked up");
  },
});

const instance = new Temporal.PlainYearMonth(2000, 5, "iso8601", 1);
instance.since(new Temporal.PlainYearMonth(1999, 4));

Object.defineProperty(Temporal.Calendar.prototype, "dateUntil", dateUntilOriginal);
