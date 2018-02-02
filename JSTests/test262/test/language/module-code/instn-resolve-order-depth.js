// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Module dependencies are resolved following a depth-first strategy
esid: sec-moduledeclarationinstantiation
negative:
  phase: resolution
  type: ReferenceError
flags: [module]
---*/

throw "Test262: This statement should not be evaluated.";

import './instn-resolve-order-depth-child_FIXTURE.js';
import './instn-resolve-order-depth-syntax_FIXTURE.js';
