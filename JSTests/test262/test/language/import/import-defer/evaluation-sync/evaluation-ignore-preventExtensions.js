// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-module-namespace-exotic-objects-preventextensions
description: >
  [[PreventExtensions]] does not trigger evaluation of the module
info: |
  [[PreventExtensions]] ( )
    1. Return **true**.

flags: [module]
features: [import-defer]
---*/

import "./setup_FIXTURE.js";

import defer * as ns1 from "./dep-1_FIXTURE.js";

assert.sameValue(globalThis.evaluations.length, 0, "import defer does not trigger evaluation");

Object.preventExtensions(ns1);

assert.sameValue(globalThis.evaluations.length, 0, "[[PreventExtensions]] does not trigger evaluation");
