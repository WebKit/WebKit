// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-class-definitions-static-semantics-early-errors
description: The `await` keyword is interpreted as an Identifier within generator function expressions
info: |
  ClassStaticBlockBody : ClassStaticBlockStatementList

  [...]
  - It is a Syntax Error if ContainsAwait of ClassStaticBlockStatementList is true.
features: [class-static-block]
---*/

var await = 0;
var fromParam, fromBody;

class C {
  static {
    (function * (x = fromParam = await) {
      fromBody = await;
    })().next();
  }
}

assert.sameValue(fromParam, 0, 'from parameter');
assert.sameValue(fromBody, 0, 'from body');
