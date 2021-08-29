// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.timezone
description: Temporal.Now.timeZone is extensible.
info: |
  ## 17 ECMAScript Standard Built-in Objects

  Unless specified otherwise, the [[Extensible]] internal slot
  of a built-in object initially has the value true.
features: [Temporal]
---*/

assert(
  Object.isExtensible(Temporal.Now.timeZone),
  'Object.isExtensible(Temporal.Now.timeZone) must return true'
);
