// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.tostring
description: Empty or a function object may be used as options
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(0n, "UTC");

const result1 = instance.toString({});
assert.sameValue(
  result1, "1970-01-01T00:00:00+00:00[UTC]",
  "options may be an empty plain object"
);

const result2 = instance.toString(() => {});
assert.sameValue(
  result2, "1970-01-01T00:00:00+00:00[UTC]",
  "options may be a function object"
);
