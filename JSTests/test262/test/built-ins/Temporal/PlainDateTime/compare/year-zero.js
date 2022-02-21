// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Negative zero, as an extended year, fails
esid: sec-temporal.plaindatetime.compare
features: [Temporal]
---*/

const ok = new Temporal.PlainDateTime(2000, 5, 2, 15);
const bad = "-000000-12-07T03:24:30";

assert.throws(RangeError,
  () => Temporal.PlainDateTime.compare(bad,ok),
  "Cannot use minus zero as extended year (first argument)"
);

assert.throws(RangeError,
  () => Temporal.PlainDateTime.compare(ok, bad),
  "Cannot use minus zero as extended year (second argument)"
);
