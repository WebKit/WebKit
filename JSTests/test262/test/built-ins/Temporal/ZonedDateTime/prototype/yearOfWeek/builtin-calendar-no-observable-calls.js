// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.yearofweek
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const yearOfWeekOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "yearOfWeek");
Object.defineProperty(Temporal.Calendar.prototype, "yearOfWeek", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("yearOfWeek should not be looked up");
  },
});

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "iso8601");
instance.yearOfWeek;

Object.defineProperty(Temporal.Calendar.prototype, "yearOfWeek", yearOfWeekOriginal);
