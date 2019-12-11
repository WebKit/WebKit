// Copyright 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: GroupSpecifiers must be unique.
info: |
  It is a Syntax Error if Pattern contains multiple GroupSpecifiers
  whose enclosed RegExpIdentifierNames have the same StringValue.
esid: sec-patterns-static-semantics-early-errors
negative:
  phase: parse
  type: SyntaxError
features: [regexp-named-groups]
---*/

$DONOTEVALUATE();

/(?<a>a)(?<b>b)(?<a>a)/u;
