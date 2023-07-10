// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: Time zone IDs are valid input for a time zone
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const getPossibleInstantsForOriginal = Object.getOwnPropertyDescriptor(Temporal.TimeZone.prototype, "getPossibleInstantsFor");
Object.defineProperty(Temporal.TimeZone.prototype, "getPossibleInstantsFor", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("getPossibleInstantsFor should not be looked up");
  },
});
const getOffsetNanosecondsForOriginal = Object.getOwnPropertyDescriptor(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor");
Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("getOffsetNanosecondsFor should not be looked up");
  },
});

const instance1 = new Temporal.ZonedDateTime(0n, new Temporal.TimeZone("UTC"));
assert(instance1.until({ year: 1970, month: 1, day: 1, timeZone: "UTC" }).blank, "Time zone created from string 'UTC'");

const instance2 = new Temporal.ZonedDateTime(0n, new Temporal.TimeZone("-01:30"));
assert(instance2.until({ year: 1969, month: 12, day: 31, hour: 22, minute: 30, timeZone: "-01:30" }).blank, "Time zone created from string '-01:30'");

Object.defineProperty(Temporal.TimeZone.prototype, "getPossibleInstantsFor", getPossibleInstantsForOriginal);
Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", getOffsetNanosecondsForOriginal);
