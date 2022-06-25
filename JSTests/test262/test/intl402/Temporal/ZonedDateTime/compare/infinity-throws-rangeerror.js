// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws if any value in a property bag for either argument is Infinity or -Infinity
esid: sec-temporal.zoneddatetime.compare
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const other = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "gregory");
const base = { era: "ad", month: 5, day: 2, hour: 15, timeZone: "UTC", calendar: "gregory" };

[Infinity, -Infinity].forEach((inf) => {
  assert.throws(RangeError, () => Temporal.ZonedDateTime.compare({ ...base, eraYear: inf }, other), `eraYear property cannot be ${inf}`);

  assert.throws(RangeError, () => Temporal.ZonedDateTime.compare(other, { ...base, eraYear: inf }), `eraYear property cannot be ${inf}`);

  const calls1 = [];
  const obj1 = TemporalHelpers.toPrimitiveObserver(calls1, inf, "eraYear");
  assert.throws(RangeError, () => Temporal.ZonedDateTime.compare({ ...base, eraYear: obj1 }, other));
  assert.compareArray(calls1, ["get eraYear.valueOf", "call eraYear.valueOf"], "it fails after fetching the primitive value");

  const calls2 = [];
  const obj2 = TemporalHelpers.toPrimitiveObserver(calls2, inf, "eraYear");
  assert.throws(RangeError, () => Temporal.ZonedDateTime.compare(other, { ...base, eraYear: obj2 }));
  assert.compareArray(calls2, ["get eraYear.valueOf", "call eraYear.valueOf"], "it fails after fetching the primitive value");
});
