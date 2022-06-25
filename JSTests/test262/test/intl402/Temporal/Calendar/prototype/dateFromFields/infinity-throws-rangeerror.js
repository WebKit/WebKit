// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws if eraYear in the property bag is Infinity or -Infinity
esid: sec-temporal.calendar.prototype.datefromfields
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("gregory");
const base = { era: "ad", month: 5, day: 2, calendar: "gregory" };

[Infinity, -Infinity].forEach((inf) => {
  ["constrain", "reject"].forEach((overflow) => {
    assert.throws(RangeError, () => instance.dateFromFields({ ...base, eraYear: inf }, { overflow }), `eraYear property cannot be ${inf} (overflow ${overflow}`);

    const calls = [];
    const obj = TemporalHelpers.toPrimitiveObserver(calls, inf, "eraYear");
    assert.throws(RangeError, () => instance.dateFromFields({ ...base, eraYear: obj }, { overflow }));
    assert.compareArray(calls, ["get eraYear.valueOf", "call eraYear.valueOf"], "it fails after fetching the primitive value");
  });
});
