// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-module-namespace-exotic-objects-get-p-receiver
description: >
  [[Get]] of a string triggers evaluation of the module
info: |
  [[Get]] ( _P_, _Receiver_ )
    1. If _P_ is a Symbol, then
      1. Return ! OrdinaryGet(_O_, _P_, _Receiver_).
    1. If _O_.[[Deferred]] is **true**, perform ? EnsureDeferredNamespaceEvaluation(_O_).
    1. ...

flags: [module]
features: [import-defer]
---*/

import "./setup_FIXTURE.js";

import defer * as ns1 from "./dep-1_FIXTURE.js";

assert.sameValue(globalThis.evaluations.length, 0, "import defer does not trigger evaluation");

ns1.foo;

assert(globalThis.evaluations.length > 0, "[[Get]] of a string triggers evaluation");
