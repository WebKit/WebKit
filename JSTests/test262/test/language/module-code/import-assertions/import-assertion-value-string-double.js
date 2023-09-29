// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  WithClause in ImportDeclaration may use a string literal as a value (delimited with U+0022)
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
negative:
  phase: resolution
  type: SyntaxError
features: [import-assertions, globalThis]
flags: [module]
---*/

$DONOTEVALUATE();

import "./ensure-linking-error_FIXTURE.js";

import x from './import-assertion-1_FIXTURE.js' assert {test262:"\u0078"};
import './import-assertion-2_FIXTURE.js' assert {test262:"\u0078"};
export * from './import-assertion-3_FIXTURE.js' assert {test262:"\u0078"};
