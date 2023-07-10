// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: >
  Call getPossibleInstantsFor with values near the date/time limit and a fixed offset.
features: [Temporal, exponentiation]
---*/

const oneHour = 1n * 60n * 60n * 1000n**3n;

const minDt = new Temporal.PlainDateTime(-271821, 4, 19, 1, 0, 0, 0, 0, 0);
const minValidDt = new Temporal.PlainDateTime(-271821, 4, 20, 0, 0, 0, 0, 0, 0);
const maxDt = new Temporal.PlainDateTime(275760, 9, 13, 0, 0, 0, 0, 0, 0);

let zero = new Temporal.TimeZone("+00");
let plusOne = new Temporal.TimeZone("+01");
let minusOne = new Temporal.TimeZone("-01");

// Try the minimum date-time.
assert.throws(RangeError, () => zero.getPossibleInstantsFor(minDt));
assert.throws(RangeError, () => plusOne.getPossibleInstantsFor(minDt));
assert.throws(RangeError, () => minusOne.getPossibleInstantsFor(minDt));

// Try the minimum valid date-time.
{
  let r = zero.getPossibleInstantsFor(minValidDt);
  assert.sameValue(r.length, 1);
  assert.sameValue(r[0].epochNanoseconds, -86_40000_00000_00000_00000n);
}

{
  let r = minusOne.getPossibleInstantsFor(minValidDt);
  assert.sameValue(r.length, 1);
  assert.sameValue(r[0].epochNanoseconds, -86_40000_00000_00000_00000n + oneHour);
}

assert.throws(RangeError, () => plusOne.getPossibleInstantsFor(minValidDt));

// Try the maximum valid date-time.
{
  let r = zero.getPossibleInstantsFor(maxDt);
  assert.sameValue(r.length, 1);
  assert.sameValue(r[0].epochNanoseconds, 86_40000_00000_00000_00000n);
}

{
  let r = plusOne.getPossibleInstantsFor(maxDt);
  assert.sameValue(r.length, 1);
  assert.sameValue(r[0].epochNanoseconds, 86_40000_00000_00000_00000n - oneHour);
}

assert.throws(RangeError, () => minusOne.getPossibleInstantsFor(maxDt));
