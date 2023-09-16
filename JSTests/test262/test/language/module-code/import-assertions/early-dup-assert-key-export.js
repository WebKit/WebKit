// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: AssertClause may not have duplicate keys (export declaration)
esid: sec-modules
info: |
  AssertClause:assert{AssertEntries,opt}

  - It is a Syntax Error if AssertClauseToAssertions of AssertClause has two
    entries a and b such that a.[[Key]] is b.[[Key]].
features: [import-assertions]
flags: [module]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

export * from './import-assertion-3_FIXTURE.js' assert {
  test262_a: '',
  test262_b: '',
  'test262_\u0061': ''
};
