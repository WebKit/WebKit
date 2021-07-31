// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: >
  Temporal.Now.instant returns an Instant describing the current moment in time
  (as corroborated by `Date.now`)
features: [Temporal, BigInt]
---*/

var nowBefore = Date.now();
var seconds = Number(Temporal.Now.instant().epochNanoseconds / 1000000n);
var nowAfter = Date.now();

assert(seconds >= nowBefore, 'before');
assert(seconds <= nowAfter, 'after');
