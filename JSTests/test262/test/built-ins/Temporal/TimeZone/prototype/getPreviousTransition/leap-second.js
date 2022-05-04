// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getprevioustransition
description: Leap second is a valid ISO string for Instant
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const arg = "2016-12-31T23:59:60Z";
const result = instance.getPreviousTransition(arg);
assert.sameValue(
  result,
  null,
  "leap second is a valid ISO string for Instant"
);
