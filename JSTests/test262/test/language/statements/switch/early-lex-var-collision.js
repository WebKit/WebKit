// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-switch-statement-static-semantics-early-errors
es6id: 13.12.1
description: >
  Syntax error from collisions between lexical variables and var-declarared
  variables
info: >
  It is a Syntax Error if any element of the LexicallyDeclaredNames of
  CaseClauses also occurs in the VarDeclaredNames of CaseClauses.
negative: SyntaxError
features: [let]
---*/

switch (0) {
  case 1:
    var test262id;
    break;
  default:
    let test262id;
}
