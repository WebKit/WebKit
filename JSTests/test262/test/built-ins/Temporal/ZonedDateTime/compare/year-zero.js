// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: Negative zero, as an extended year, fails
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(0n, "UTC");
const bad = "-000000-08-19T17:30Z";

assert.throws(RangeError,
  () => Temporal.ZonedDateTime.compare(bad, instance),
  "cannot use negative zero as extended year (first argument)"
);
assert.throws(RangeError,
  () => Temporal.ZonedDateTime.compare(instance, bad),
  "cannot use negative zero as extended year (second argument)"
);
