// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.compare
description: Invalid calendar string should throw RangeError
features: [Temporal]
---*/

const invalidStrings = [
  ["", "empty string"],
  ["1997-12-04[u-ca=notacal]", "Unknown calendar"],
];

for (const [arg, description] of invalidStrings) {
  assert.throws(
    RangeError,
    () => Temporal.PlainDate.compare(arg, new Temporal.PlainDate(1976, 11, 18)),
    `${description} is not a valid calendar ID (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainDate.compare(new Temporal.PlainDate(1976, 11, 18), arg),
    `${description} is not a valid calendar ID (second argument)`
  );
}
