// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withtimezone
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

const instance = new Temporal.ZonedDateTime(0n, "UTC");

["UTC", "+01:30"].forEach((timeZone) => {
  const result = instance.withTimeZone(timeZone);
  assert.sameValue(result.getISOFields().timeZone, timeZone, `time zone slot should store string "${timeZone}"`);
});

Object.defineProperty(Temporal.TimeZone.prototype, "getPossibleInstantsFor", getPossibleInstantsForOriginal);
Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", getOffsetNanosecondsForOriginal);
