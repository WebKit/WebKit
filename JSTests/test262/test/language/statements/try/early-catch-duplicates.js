// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-try-statement-static-semantics-early-errors
es6id: 13.15.1
description: >
    It is a Syntax Error if BoundNames of CatchParameter contains any duplicate
    elements.
negative:
  phase: parse
  type: SyntaxError
---*/

throw "Test262: This statement should not be evaluated.";

$ERROR('This code should not be executed.');

try { } catch ([x, x]) {}
