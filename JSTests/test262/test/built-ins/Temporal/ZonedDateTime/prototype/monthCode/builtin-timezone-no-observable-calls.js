// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.monthcode
description: >
  Calling the method on an instance constructed with a builtin time zone causes
  no observable lookups or calls to time zone methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const getOffsetNanosecondsForOriginal = Object.getOwnPropertyDescriptor(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor");
Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("getOffsetNanosecondsFor should not be looked up");
  },
});

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "iso8601");
instance.monthCode;

Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", getOffsetNanosecondsForOriginal);
