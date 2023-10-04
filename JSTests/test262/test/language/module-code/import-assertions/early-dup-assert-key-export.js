// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: WithClause may not have duplicate keys (export declaration)
esid: sec-modules
info: |
  WithClause: AttributesKeyword { WithEntries,opt }

  - It is a Syntax Error if WithClauseToAttributes of WithClause has two
    entries a and b such that a.[[Key]] is b.[[Key]].
features: [import-assertions]
flags: [module]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

export * from './import-assertion-3_FIXTURE.js' assert {
  type: 'json',
  'typ\u0065': ''
};
