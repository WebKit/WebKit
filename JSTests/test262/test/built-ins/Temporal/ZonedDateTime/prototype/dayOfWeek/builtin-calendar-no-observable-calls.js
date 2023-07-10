// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.dayofweek
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

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "iso8601");
instance.dayOfWeek;

Object.defineProperty(Temporal.Calendar.prototype, "dayOfWeek", dayOfWeekOriginal);
