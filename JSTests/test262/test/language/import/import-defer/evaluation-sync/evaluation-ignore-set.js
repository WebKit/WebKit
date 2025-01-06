// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-module-namespace-exotic-objects-set-p-v-receiver
description: >
  [[Set]] does not trigger evaluation of the module
info: |
  [[Set]] ( _P_, _V_, _Receiver_ )
    1. Return **false**.

flags: [module]
features: [import-defer]
---*/

import "./setup_FIXTURE.js";

import defer * as ns1 from "./dep-1_FIXTURE.js";

assert.sameValue(globalThis.evaluations.length, 0, "import defer does not trigger evaluation");

try {
  ns1.foo = 2;
} catch {}
try {
  ns1.ns_1_2 = 3;
} catch {}

assert.sameValue(globalThis.evaluations.length, 0, "[[Set]] of a symbol does not trigger evaluation");
