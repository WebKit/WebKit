// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: Temporal.now.instant returns an instance of the Instant constructor
features: [Temporal]
---*/

assert.sameValue(
  Object.getPrototypeOf(Temporal.now.instant()),
  Temporal.Instant.prototype
);
