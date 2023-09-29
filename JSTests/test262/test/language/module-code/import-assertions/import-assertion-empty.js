// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: WithClause in ImportDeclaration may be empty
esid: sec-modules
info: |
  ImportDeclaration:
    import ModuleSpecifier[no LineTerminator here] WithClause;

  WithClause:
    assert {}
    assert {WithEntries ,opt}

  WithEntries:
    AttributeKey : StringLiteral
    AttributeKey : StringLiteral , WithEntries

  AttributeKey:
    IdentifierName
    StringLiteral
features: [import-assertions, globalThis]
flags: [module]
---*/

import x from './import-assertion-1_FIXTURE.js' assert {};
import './import-assertion-2_FIXTURE.js' assert {};
export * from './import-assertion-3_FIXTURE.js' assert {};

assert.sameValue(x, 262.1);
