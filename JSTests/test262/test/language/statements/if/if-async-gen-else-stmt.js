// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-if-statement
description: >
  AsyncGeneratorDeclaration is not allowed in statement position
info: |
  ExpressionStatement[Yield, Await] :
    [lookahead ∉ { {, function, async [no LineTerminator here] function, class, let [ }]
    Expression[+In, ?Yield, ?Await] ;
negative:
  phase: parse
  type: SyntaxError
features: [async-iteration]
---*/

$DONOTEVALUATE();

if (true) async function* f() {  } else ;
