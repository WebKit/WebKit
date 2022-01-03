// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: Specifically disallowed units for the smallestUnit option
features: [Temporal, arrow-function]
---*/

const instance = new Temporal.Duration(0, 0, 0, 4, 5, 6, 7, 987, 654, 321);
const invalidUnits = [
  "era",
  "eras",
];
invalidUnits.forEach((smallestUnit) => {
  assert.throws(
    RangeError,
    () => instance.round({ smallestUnit }),
    `{ smallestUnit: "${smallestUnit}" } should not be allowed as an argument to round`
  );
  assert.throws(
    RangeError,
    () => instance.round(smallestUnit),
    `"${smallestUnit}" should not be allowed as an argument to round`
  );
});
