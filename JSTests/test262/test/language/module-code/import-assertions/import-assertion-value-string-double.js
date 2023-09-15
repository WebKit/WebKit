// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  AssertClause in ImportDeclaration may use a string literal as a value (delimited with U+0022)
esid: sec-modules
info: |
  ImportDeclaration:
    import ModuleSpecifier[no LineTerminator here] AssertClause;

  AssertClause:
    assert {}
    assert {AssertEntries ,opt}

  AssertEntries:
    AssertionKey : StringLiteral
    AssertionKey : StringLiteral , AssertEntries

  AssertionKey:
    IdentifierName
    StringLiteral
features: [import-assertions, globalThis]
flags: [module]
---*/

import x from './import-assertion-1_FIXTURE.js' assert {test262:"\u0078"};
import './import-assertion-2_FIXTURE.js' assert {test262:"\u0078"};
export * from './import-assertion-3_FIXTURE.js' assert {test262:"\u0078"};

assert.sameValue(x, 262.1);
assert.sameValue(globalThis.test262, 262.2);
