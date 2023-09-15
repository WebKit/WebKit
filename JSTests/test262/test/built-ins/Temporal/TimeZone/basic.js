// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone
description: Basic tests for the Temporal.TimeZone constructor.
features: [Temporal]
---*/

const valid = [
  ["+01:00"],
  ["-01:00"],
  ["+0330", "+03:30"],
  ["-0650", "-06:50"],
  ["-08", "-08:00"],
  ["\u221201:00", "-01:00"],
  ["\u22120650", "-06:50"],
  ["\u221208", "-08:00"],
  ["UTC"],
];
for (const [zone, id = zone] of valid) {
  const result = new Temporal.TimeZone(zone);
  assert.sameValue(typeof result, "object", `object should be created for ${zone}`);
  assert.sameValue(result.id, id, `id for ${zone} should be ${id}`);
}

const invalid = ["+00:01.1", "-01.1", "+01:00:00", "-010000", "+03:30:00.000000001", "-033000.1"];
for (const zone of invalid) {
  assert.throws(RangeError, () => new Temporal.TimeZone(zone), `should throw for ${zone}`);
}
