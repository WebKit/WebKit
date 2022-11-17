// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Various forms of unknown annotation
features: [Temporal]
---*/

const tests = [
  ["1970-01-01T00:00Z[foo=bar]", "alone"],
  ["1970-01-01T00:00Z[UTC][foo=bar]", "with time zone"],
  ["1970-01-01T00:00Z[u-ca=iso8601][foo=bar]", "with calendar"],
  ["1970-01-01T00:00Z[UTC][foo=bar][u-ca=iso8601]", "with time zone and calendar"],
  ["1970-01-01T00:00Z[foo=bar][_foo-bar0=Ignore-This-999999999999]", "with another unknown annotation"],
];

const instance = new Temporal.TimeZone("UTC");

tests.forEach(([arg, description]) => {
  const result = instance.getOffsetStringFor(arg);

  assert.sameValue(
    result,
    "+00:00",
    `unknown annotation (${description})`
  );
});
