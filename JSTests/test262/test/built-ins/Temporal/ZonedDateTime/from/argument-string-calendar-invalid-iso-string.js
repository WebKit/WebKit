// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Invalid calendar string should throw RangeError
features: [Temporal]
---*/

const timeZone = "UTC";

const invalidStrings = [
  ["", "empty string"],
  ["1997-12-04[u-ca=notacal]", "Unknown calendar"],
];

for (const [arg, description] of invalidStrings) {
  assert.throws(
    RangeError,
    () => Temporal.ZonedDateTime.from(arg),
    `${description} is not a valid calendar ID`
  );
}
