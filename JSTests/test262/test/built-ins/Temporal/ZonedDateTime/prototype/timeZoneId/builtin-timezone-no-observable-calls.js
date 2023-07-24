// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.timezoneid
description: >
  Calling the method on an instance constructed with a builtin time zone causes
  no observable lookups or calls to time zone methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const idOriginal = Object.getOwnPropertyDescriptor(Temporal.TimeZone.prototype, "id");
Object.defineProperty(Temporal.TimeZone.prototype, "id", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("id should not be looked up");
  },
});

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "iso8601");
instance.timeZoneId;

Object.defineProperty(Temporal.TimeZone.prototype, "id", idOriginal);
