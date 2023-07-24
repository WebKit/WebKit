// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
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

// The following are all valid strings so should not throw:

["UTC", "+01:00"].forEach((timeZone) => {
  Temporal.Now.plainDateTime("iso8601", timeZone);
});

Object.defineProperty(Temporal.TimeZone.prototype, "getPossibleInstantsFor", getPossibleInstantsForOriginal);
Object.defineProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", getOffsetNanosecondsForOriginal);
