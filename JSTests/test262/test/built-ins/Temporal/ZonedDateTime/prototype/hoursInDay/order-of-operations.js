// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.hoursinday
description: User code calls happen in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  // GetPlainDateTimeFor
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // GetInstantFor
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // GetInstantFor
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
];

// Time zone with special requirements for testing DisambiguatePossibleInstants:
// midnight 1970-01-01 and 1970-01-02 are each in the middle of a fall-back
// transition of 1 h. Midnight 1970-01-03 and 1970-01-04 are each in the middle
// of a spring-forward transition of 1 h.
// The fall-back transitions occur 30 minutes after the day boundaries at local
// time: at epoch seconds 1800 and 91800. the spring-forward transitions occur
// 30 minutes before the day boundaries at local time: at epoch seconds 167400
// and 257400.
// This is because calculating the hours in the instance's day requires calling
// getPossibleInstantsFor on both the preceding local midnight and the following
// local midnight.
const timeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
  getOffsetNanosecondsFor(instant) {
    const epochNs = instant.epochNanoseconds;
    if (epochNs < 1800_000_000_000n) return 0;
    if (epochNs < 91800_000_000_000n) return 3600_000_000_000;
    if (epochNs < 167400_000_000_000n) return 7200_000_000_000;
    if (epochNs < 257400_000_000_000n) return 3600_000_000_000;
    return 0;
  },
  getPossibleInstantsFor(dt) {
    const cmp = Temporal.PlainDateTime.compare;

    const zero = new Temporal.TimeZone("+00:00").getInstantFor(dt);
    const one = new Temporal.TimeZone("+01:00").getInstantFor(dt);
    const two = new Temporal.TimeZone("+02:00").getInstantFor(dt);

    const fallBackLocalOne = new Temporal.PlainDateTime(1970, 1, 1, 0, 30);
    const fallBackLocalTwo = new Temporal.PlainDateTime(1970, 1, 2, 0, 30);
    const springForwardLocalOne = new Temporal.PlainDateTime(1970, 1, 2, 23, 30);
    const springForwardLocalTwo = new Temporal.PlainDateTime(1970, 1, 3, 23, 30);

    if (cmp(dt, fallBackLocalOne) < 0) return [zero];
    if (cmp(dt, fallBackLocalOne.add({ hours: 1 })) < 0) return [zero, one];
    if (cmp(dt, fallBackLocalTwo) < 0) return [one];
    if (cmp(dt, fallBackLocalTwo.add({ hours: 1 })) < 0) return [one, two];
    if (cmp(dt, springForwardLocalOne) < 0) return [two];
    if (cmp(dt, springForwardLocalOne.add({ hours: 1 })) < 0) return [];
    if (cmp(dt, springForwardLocalTwo) < 0) return [one];
    if (cmp(dt, springForwardLocalTwo.add({ hours: 1 })) < 0) return [];
    return [zero];
  },
});

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone, calendar);
const fallBackInstance = new Temporal.ZonedDateTime(43200_000_000_000n /* 1970-01-01T12:00 */, timeZone, calendar);
const springForwardInstance = new Temporal.ZonedDateTime(216000_000_000_000n /* 1970-01-03T12:00 */, timeZone, calendar);
actual.splice(0); // clear calls that happened in constructors

instance.hoursInDay;
assert.compareArray(actual, expected, "order of operations with both midnights at normal wall-clock times");
actual.splice(0); // clear

fallBackInstance.hoursInDay;
assert.compareArray(actual, expected, "order of operations with both midnights at repeated wall-clock times");
actual.splice(0); // clear

springForwardInstance.hoursInDay;
assert.compareArray(actual, [
  // GetPlainDateTimeFor
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // GetInstantFor
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // Note, no call to dateAdd as addition takes place in the ISO calendar
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // GetInstantFor
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // Note, no call to dateAdd here either
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
], "order of operations with both midnights at skipped wall-clock times");
actual.splice(0); // clear
