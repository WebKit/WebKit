// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Negative zero, as an extended year, fails
esid: sec-temporal.plaindate.compare
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);
const bad = "-000000-08-24";

assert.throws(RangeError,
  () => Temporal.PlainDate.compare(bad, instance),
  "Minus zero is an invalid extended year (first argument)"
);

assert.throws(RangeError,
  () => Temporal.PlainDate.compare(instance, bad),
  "Minus zero is an invalid extended year (second argument)"
);
