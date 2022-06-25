// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.2.1
description: >
    ArrowParameters[Yield] :
      ...
      CoverParenthesizedExpressionAndArrowParameterList[?Yield]

    CoverParenthesizedExpressionAndArrowParameterList, refined by:

    ArrowFormalParameters[Yield, GeneratorParameter] :
      ( StrictFormalParameters[?Yield, ?GeneratorParameter] )

    No parameters named "eval"

negative:
  phase: parse
  type: SyntaxError
flags: [onlyStrict]
---*/

$DONOTEVALUATE();
var af = (eval) => 1;
