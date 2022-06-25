// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Throw a TypeError if the fields is not an object
features: [Symbol, Temporal]
---*/

const tests = [undefined, null, true, false, "string", Symbol("sym"), Math.PI, Infinity, NaN, 42n];
const iso = Temporal.Calendar.from("iso8601");
for (const fields of tests) {
  assert.throws(TypeError, () => iso.yearMonthFromFields(fields, {}));
}
