// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.with
description: TypeError thrown when options argument is a primitive
features: [BigInt, Symbol, Temporal]
---*/

const values = [
  null,
  true,
  "2021-01",
  Symbol(),
  1,
  2n,
];

const ym = Temporal.PlainYearMonth.from("2019-10");
values.forEach((value) => {
  assert.throws(TypeError, () => ym.with({ year: 2020 }, value), `TypeError on wrong argument type ${typeof value}`);
});
