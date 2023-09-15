// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: Exceptions thrown if a value is passed that converts to an invalid string
features: [Temporal]
---*/

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
  7,
  4.2,
  12n
];

const tzUTC = new Temporal.TimeZone("UTC");
for (const primitive of primitives) {
  assert.throws(typeof primitive === "string" ? RangeError : TypeError, () => tzUTC.equals(primitive));
}

const symbol = Symbol();
assert.throws(TypeError, () => tzUTC.equals(symbol));
