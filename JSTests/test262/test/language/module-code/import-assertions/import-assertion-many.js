// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    AssertClause in ImportDeclaration may contain multiple AssertEntries
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

import x from './import-assertion-1_FIXTURE.js' assert {test262_1:'',test262_2:'',test262_3:'',test262_4:''};
import './import-assertion-2_FIXTURE.js' assert {test262_1:'',test262_2:'',test262_3:'',test262_4:''};
export * from './import-assertion-3_FIXTURE.js' assert {test262_1:'',test262_2:'',test262_3:'',test262_4:''};

assert.sameValue(x, 262.1);
assert.sameValue(globalThis.test262, 262.2);
