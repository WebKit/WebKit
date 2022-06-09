// Copyright 2020 Salesforce.com, Inc. All rights reserved.
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

  It is a Syntax Error if any code matches this production.
features: [optional-chaining]
negative:
  type: SyntaxError
  phase: parse
---*/

$DONOTEVALUATE();

const a = function() {};

a?.`hello`;
