// Copyright (C) 2026 itsu-dev. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  An unparenthesized `in` expression is not permitted in an arrow function
  concise expression body when the arrow function occurs in a for statement
  variable initializer parsed with [~In].
info: |
  ForStatement :
    for ( var VariableDeclarationList[~In, ?Yield, ?Await] ;
          Expression[+In, ?Yield, ?Await]opt ;
          Expression[+In, ?Yield, ?Await]opt ) Statement

  ArrowFunction[In, Yield, Await] :
    ArrowParameters[?Yield, ?Await] => ConciseBody[?In]

  ConciseBody[In] :
    [lookahead != {] ExpressionBody[?In, ~Await]
negative:
  phase: parse
  type: SyntaxError
esid: sec-for-statement
features: [arrow-function]
---*/

$DONOTEVALUATE();

for (var f = () => "x" in {}; false;) {}
