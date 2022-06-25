// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: Leap second is a valid ISO string for PlainDateTime
includes: [compareArray.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

let arg = "2016-12-31T23:59:60";
const result1 = instance.getPossibleInstantsFor(arg);
assert.compareArray(
  result1.map(i => i.epochNanoseconds),
  [1_483_228_799_000_000_000n],
  "leap second is a valid ISO string for PlainDateTime"
);

arg = { year: 2016, month: 12, day: 31, hour: 23, minute: 59, second: 60 };
const result2 = instance.getPossibleInstantsFor(arg);
assert.compareArray(
  result2.map(i => i.epochNanoseconds),
  [1_483_228_799_000_000_000n],
  "second: 60 is ignored in property bag for PlainDateTime"
);
