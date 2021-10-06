// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws if any value in a property bag for either argument is Infinity or -Infinity
esid: sec-temporal.calendar.prototype.dateuntil
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");
const other = new Temporal.PlainDate(2001, 6, 3);
const base = { year: 2000, month: 5, day: 2 };

[Infinity, -Infinity].forEach((inf) => {
  ["year", "month", "day"].forEach((prop) => {
    assert.throws(RangeError, () => instance.dateUntil({ ...base, [prop]: inf }, other), `${prop} property cannot be ${inf}`);

    assert.throws(RangeError, () => instance.dateUntil(other, { ...base, [prop]: inf }), `${prop} property cannot be ${inf}`);

    const calls1 = [];
    const obj1 = TemporalHelpers.toPrimitiveObserver(calls1, inf, prop);
    assert.throws(RangeError, () => instance.dateUntil({ ...base, [prop]: obj1 }, other));
    assert.compareArray(calls1, [`get ${prop}.valueOf`, `call ${prop}.valueOf`], "it fails after fetching the primitive value");

    const calls2 = [];
    const obj2 = TemporalHelpers.toPrimitiveObserver(calls2, inf, prop);
    assert.throws(RangeError, () => instance.dateUntil(other, { ...base, [prop]: obj2 }));
    assert.compareArray(calls2, [`get ${prop}.valueOf`, `call ${prop}.valueOf`], "it fails after fetching the primitive value");
  });
});
