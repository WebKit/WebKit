// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Mixed positive and negative values or missing properties always throw
features: [Temporal]
---*/

const ym = Temporal.PlainYearMonth.from("2019-11");
for (const overflow of ["constrain", "reject"]) {
  assert.throws(RangeError, () => ym.add({ years: 1, months: -6 }, { overflow }), overflow)
}

assert.throws(TypeError, () => ym.add({}), "no properties");
assert.throws(TypeError, () => ym.add({ month: 12 }), "only singular 'month' property");
