// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Temporal.Now.zonedDateTime is extensible.
features: [Temporal]
---*/

assert(
  Object.isExtensible(Temporal.Now.zonedDateTime),
  'Object.isExtensible(Temporal.Now.zonedDateTime) must return true'
);
