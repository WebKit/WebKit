// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: Various forms of unknown annotation
features: [Temporal]
---*/

const tests = [
  ["2000-05-02[foo=bar]", "without time"],
  ["2000-05-02T15:23[foo=bar]", "alone"],
  ["2000-05-02T15:23[UTC][foo=bar]", "with time zone"],
  ["2000-05-02T15:23[u-ca=iso8601][foo=bar]", "with calendar"],
  ["2000-05-02T15:23[UTC][foo=bar][u-ca=iso8601]", "with time zone and calendar"],
  ["2000-05-02T15:23[foo=bar][_foo-bar0=Ignore-This-999999999999]", "with another unknown annotation"],
];

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

tests.forEach(([arg, description]) => {
  const result = instance.withPlainDate(arg);

  assert.sameValue(
    result.epochNanoseconds,
    957_225_600_000_000_000n,
    `unknown annotation (${description})`
  );
});
