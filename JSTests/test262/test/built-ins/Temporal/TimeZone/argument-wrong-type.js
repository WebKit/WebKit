// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone
description: RangeError thrown when constructor invoked with the wrong type
features: [Temporal]
---*/

const tests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [19761118, "number that would convert to a valid ISO string in other contexts"],
  [1n, "bigint"],
  [Symbol(), "symbol"],
  [{}, "object not implementing any protocol"],
  [new Temporal.Calendar("iso8601"), "calendar instance"],
  [new Temporal.TimeZone("UTC"), "time zone instance"],
  [Temporal.ZonedDateTime.from("2020-01-01T00:00Z[UTC]"), "ZonedDateTime instance"],
];

for (const [arg, description] of tests) {
  assert.throws(
    typeof (arg) === "string" ? RangeError : TypeError,
    () => new Temporal.TimeZone(arg),
    `${description} is not accepted by this constructor`
  );
}
