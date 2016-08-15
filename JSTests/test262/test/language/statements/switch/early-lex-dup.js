// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-switch-statement-static-semantics-early-errors
es6id: 13.12.1
description: Syntax error from duplicate lexical variables
info: >
  It is a Syntax Error if the LexicallyDeclaredNames of CaseBlock contains any
  duplicate entries.
negative: SyntaxError
features: [let, const]
---*/

switch (0) {
  case 1:
    let test262id;
    break;
  default:
    const test262id = null;
}
