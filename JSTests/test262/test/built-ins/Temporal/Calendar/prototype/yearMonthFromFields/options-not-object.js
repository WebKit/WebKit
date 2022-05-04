// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Throw a TypeError if options is not an object or undefined
info: |
  5. Set options to ? GetOptionsObject(options).
features: [Symbol, Temporal]
---*/

const tests = [null, true, false, "string", Symbol("sym"), Math.PI, Infinity, NaN, 42n];
const iso = new Temporal.Calendar("iso8601");
for (const options of tests) {
  assert.throws(TypeError, () => iso.yearMonthFromFields({ year: 2021, month: 7 }, options),
    "options is not object");
}
