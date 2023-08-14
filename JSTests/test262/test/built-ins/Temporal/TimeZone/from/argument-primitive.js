// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: RangeError thrown if a value is passed that converts to an invalid string
features: [Temporal]
---*/

class CustomTimeZone extends Temporal.TimeZone {}

const primitives = [
  undefined,
  null,
  true,
  "string",
  "local",
  "Z",
  "-00:00[UTC]",
  "+00:01.1",
  "-01.1",
  "1994-11-05T08:15:30+25:00",
  "1994-11-05T13:15:30-25:00",
  "+01:00:00",
  "-010000",
  "+03:30:00.000000001",
  "-033000.1",
  7,
  4.2,
  12n,
];

const thisValues = [
  Temporal.TimeZone,
  CustomTimeZone,
  {},
  null,
  undefined,
  7,
];

for (const thisValue of thisValues) {
  for (const primitive of primitives) {
    assert.throws(typeof primitive === 'string' ? RangeError : TypeError, () => Temporal.TimeZone.from.call(thisValue, primitive));
  }

  const symbol = Symbol();
  assert.throws(TypeError, () => Temporal.TimeZone.from.call(thisValue, symbol));
}
