// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: TimeZone.getPossibleInstantsFor called after processing timeZone and options
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.timeZone",
  "get options.disambiguation",
  "get disambiguation.toString",
  "call disambiguation.toString",
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
];

Object.defineProperty(Temporal.TimeZone, "from", {
  get() {
    actual.push("get Temporal.TimeZone.from");
    return undefined;
  },
});

const dateTime = Temporal.PlainDateTime.from("1975-02-02T14:25:36.123456789");
const instant = Temporal.Instant.fromEpochNanoseconds(-205156799012345679n);

const options = new Proxy({
  disambiguation: TemporalHelpers.toPrimitiveObserver(actual, "reject", "disambiguation"),
}, {
  has(target, property) {
    actual.push(`has options.${property}`);
    return property in target;
  },
  get(target, property) {
    actual.push(`get options.${property}`);
    return target[property];
  },
});

const timeZone = new Proxy({
  getPossibleInstantsFor(dateTimeArg) {
    actual.push("call timeZone.getPossibleInstantsFor");
    assert.sameValue(dateTimeArg, dateTime);
    return [instant];
  },
}, {
  has(target, property) {
    actual.push(`has timeZone.${property}`);
    return property in target;
  },
  get(target, property) {
    actual.push(`get timeZone.${property}`);
    return target[property];
  },
});

const result = dateTime.toZonedDateTime(timeZone, options);
assert.sameValue(result.epochNanoseconds, instant.epochNanoseconds);
assert.sameValue(result.timeZone, timeZone);
assert.sameValue(result.calendar, dateTime.calendar);

assert.compareArray(actual, expected);
