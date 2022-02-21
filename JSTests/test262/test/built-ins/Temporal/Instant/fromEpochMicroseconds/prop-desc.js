// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.fromepochmicroseconds
description: The "fromEpochMicroseconds" property of Temporal.Instant
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Instant.fromEpochMicroseconds,
  "function",
  "`typeof Instant.fromEpochMicroseconds` is `function`"
);

verifyProperty(Temporal.Instant, "fromEpochMicroseconds", {
  writable: true,
  enumerable: false,
  configurable: true,
});
