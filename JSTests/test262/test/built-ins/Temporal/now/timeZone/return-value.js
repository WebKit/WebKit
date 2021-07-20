// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.timezone
description: Temporal.now.timeZone returns an instance of the TimeZone constructor
info: |
  1. Return ? SystemTimeZone().
features: [Temporal]
---*/

assert.sameValue(
  Object.getPrototypeOf(Temporal.now.timeZone()),
  Temporal.TimeZone.prototype
);
