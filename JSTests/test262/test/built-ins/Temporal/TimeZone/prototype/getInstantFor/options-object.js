// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Empty or a function object may be used as options
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const result1 = instance.getInstantFor(new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38), {});
assert.sameValue(
  result1.epochNanoseconds, 1572345998000000000n,
  "options may be an empty plain object"
);

const result2 = instance.getInstantFor(new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38), () => {});
assert.sameValue(
  result2.epochNanoseconds, 1572345998000000000n,
  "options may be a function object"
);
