// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetimeiso
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

const instance = new Temporal.Instant(0n);

["UTC", "+01:30"].forEach((timeZone) => {
  const result = instance.toZonedDateTimeISO(timeZone);
  assert.sameValue(result.getISOFields().timeZone, timeZone, `time zone slot should store string "${timeZone}"`);
});

Object.defineProperty(Temporal.TimeZone.prototype, "getPossibleInstantsFor", getPossibleInstantsForOriginal);
Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", getOffsetNanosecondsForOriginal);
