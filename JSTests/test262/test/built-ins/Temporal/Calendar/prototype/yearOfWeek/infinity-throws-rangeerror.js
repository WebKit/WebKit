// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws if any value in the property bag is Infinity or -Infinity
esid: sec-temporal.calendar.prototype.yearofweek
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");
const base = { year: 2000, month: 5, day: 2 };

[Infinity, -Infinity].forEach((inf) => {
  ["year", "month", "day"].forEach((prop) => {
    assert.throws(RangeError, () => instance.yearOfWeek({ ...base, [prop]: inf }), `${prop} property cannot be ${inf}`);

    const calls = [];
    const obj = TemporalHelpers.toPrimitiveObserver(calls, inf, prop);
    assert.throws(RangeError, () => instance.yearOfWeek({ ...base, [prop]: obj }));
    assert.compareArray(calls, [`get ${prop}.valueOf`, `call ${prop}.valueOf`], "it fails after fetching the primitive value");
  });
});
