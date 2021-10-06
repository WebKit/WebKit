// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Throw a TypeError if the fields is not an object
info: |
  4. If Type(_fields_) is not Object, throw a *TypeError* exception.
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

const tests = [undefined, null, true, false, "string", Symbol("sym"), Infinity, NaN, Math.PI, 42n];
const iso = Temporal.Calendar.from("iso8601");
for (const fields of tests) {
  assert.throws(
    TypeError,
    () => iso.dateFromFields(fields, {})
    `dateFromFields(${typeof fields}) throws a TypeError exception`
  );
}
