// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.NumberFormat.prototype.formatRange
description: >
  "formatRange" basic tests when argument  x > y, BigInt included and covers PartitionNumberRangePattern throw a RangeError exception.
info: |
  1.1.21 PartitionNumberRangePattern( numberFormat, x, y )
  (...)
  1.1.21_2.a. If y is a mathematical value and y < x, throw a RangeError exception.
  1.1.21._2.b if y is -∞, throw a RangeError exception.
  1.1.21._2.c if y is -0 and x ≥ 0, throw a RangeError exception.
  (...)
  1.1.21._3.a if y is a mathematical value, throw a RangeError exception
  1.1.21._3.b if y is -∞, throw a RangeError exception.
  1.1.21._3.c if y is -0, throw a RangeError exception.
  (...)
  1.1.21_4.a if y is a mathematical value and y < 0, throw a RangeError exception.
  1.1.21_4.b if y is -∞, throw a RangeError exception.
  features: [Intl.NumberFormat-v3]
---*/

const nf = new Intl.NumberFormat();

// If x > y, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(23, 12) });

//  1.1.21_2 (...)
// If x > y, throw a RangeError exception and both x and y are bigint.
assert.throws(RangeError, () => { nf.formatRange(23n, 12n) });
//if y is -∞, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(23, -Infinity) });
//if y is -0 and x ≥ 0, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(23, -0) });
assert.throws(RangeError, () => { nf.formatRange(0, -0) });

//  1.1.21_3 (...)
// if y is a mathematical value, throw a RangeError exception
assert.throws(RangeError, () => { nf.formatRange(Infinity, 23) });
// if y is -∞, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(Infinity, -Infinity) });
// if y is -0, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(Infinity, -0) });

//  1.1.21_4 (...)
// if y is a mathematical value and y < 0, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(-0, -1) });
// if y is -∞, throw a RangeError exception.
assert.throws(RangeError, () => { nf.formatRange(-0, -Infinity) });

