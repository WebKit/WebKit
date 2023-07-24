// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.daysinyear
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

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "iso8601");
instance.daysInYear;

Object.defineProperty(Temporal.Calendar.prototype, "daysInYear", daysInYearOriginal);
