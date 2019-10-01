// Copyright 2019 Google, Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: prod-OptionalExpression
description: >
  template string passed to tail position of optional chain
info: |
  Static Semantics: Early Errors
    OptionalChain:
      ?.TemplateLiteral
      OptionalChain TemplateLiteral
features: [optional-chaining]
negative:
  type: SyntaxError
  phase: parse
---*/

$DONOTEVALUATE();

const a = {fn() {}};

// This production exists in order to prevent automatic semicolon
// insertion rules.
a?.fn
  `hello`
