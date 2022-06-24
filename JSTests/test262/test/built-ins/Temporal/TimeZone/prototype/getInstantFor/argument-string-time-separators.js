// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Time separator in string argument can vary
features: [Temporal]
---*/

const tests = [
  ["1976-11-18T15:23", "uppercase T"],
  ["1976-11-18t15:23", "lowercase T"],
  ["1976-11-18 15:23", "space between date and time"],
];

const instance = new Temporal.TimeZone("UTC");

tests.forEach(([arg, description]) => {
  const result = instance.getInstantFor(arg);

  assert.sameValue(
    result.epochNanoseconds,
    217_178_580_000_000_000n,
    `variant time separators (${description})`
  );
});
