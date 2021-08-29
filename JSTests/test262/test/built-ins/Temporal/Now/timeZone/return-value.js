// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.timezone
description: Temporal.Now.timeZone returns an instance of the TimeZone constructor
info: |
  1. Return ? SystemTimeZone().
features: [Temporal]
---*/

assert.sameValue(
  Object.getPrototypeOf(Temporal.Now.timeZone()),
  Temporal.TimeZone.prototype,
  'Object.getPrototypeOf(Temporal.Now.timeZone()) returns Temporal.TimeZone.prototype'
);
