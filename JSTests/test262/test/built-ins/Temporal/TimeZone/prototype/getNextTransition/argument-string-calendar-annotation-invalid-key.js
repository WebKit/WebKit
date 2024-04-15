// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getnexttransition
description: Annotation keys are lowercase-only
features: [Temporal]
---*/

const invalidStrings = [
  ["1970-01-01T00:00Z[U-CA=iso8601]", "invalid capitalized key"],
  ["1970-01-01T00:00Z[u-CA=iso8601]", "invalid partially-capitalized key"],
  ["1970-01-01T00:00Z[FOO=bar]", "invalid capitalized unrecognized key"],
];
const instance = new Temporal.TimeZone("UTC");
invalidStrings.forEach(([arg, descr]) => {
  assert.throws(
    RangeError,
    () => instance.getNextTransition(arg),
    `annotation keys must be lowercase: ${arg} - ${descr}`
  );
});
