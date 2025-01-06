// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-module-namespace-exotic-objects-isextensible
description: >
  [[IsExtensible]] does not trigger evaluation of the module
info: |
  [[IsExtensible]] ( )
    1. Return **false**.

flags: [module]
features: [import-defer]
---*/

import "./setup_FIXTURE.js";

import defer * as ns1 from "./dep-1_FIXTURE.js";

assert.sameValue(globalThis.evaluations.length, 0, "import defer does not trigger evaluation");

Object.isExtensible(ns1);

assert.sameValue(globalThis.evaluations.length, 0, "[[IsExtensible]] does not trigger evaluation");
