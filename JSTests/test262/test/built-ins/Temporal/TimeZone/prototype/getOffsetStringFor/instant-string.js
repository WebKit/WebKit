// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Conversion of ISO date-time strings to Temporal.Instant instances
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

let str = "1970-01-01T00:00";
assert.throws(RangeError, () => instance.getOffsetStringFor(str), "bare date-time string is not an instant");
str = "1970-01-01T00:00[UTC]";
assert.throws(RangeError, () => instance.getOffsetStringFor(str), "date-time + IANA annotation is not an instant");

// The following are all valid strings so should not throw:

const valids = [
  "1970-01-01T00:00Z",
  "1970-01-01T00:00+01:00",
  "1970-01-01T00:00Z[UTC]",
  "1970-01-01T00:00+01:00[UTC]",
  "1970-01-01T00:00Z[u-ca=hebrew]",
  "1970-01-01T00:00+01:00[u-ca=hebrew]",
  "1970-01-01T00:00+01:00[Etc/Ignored][u-ca=hebrew]",
];
for (const str of valids) {
  const result = instance.getOffsetStringFor(str);
  assert.sameValue(result, "+00:00");
}
