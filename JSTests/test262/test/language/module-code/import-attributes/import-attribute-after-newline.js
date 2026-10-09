// Copyright (C) 2026 Apple Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Newline is allowed before WithClause
esid: sec-modules
info: |
  ImportDeclaration:
    import ImportClause FromClause WithClause;
    import ModuleSpecifier WithClause;

  ExportDeclaration:
    export ExportFromClause FromClause WithClause;
features: [import-attributes, globalThis]
flags: [module]
---*/

import './import-attribute-1_FIXTURE.js'
with {};

import x from './import-attribute-1_FIXTURE.js'
with {};
assert.sameValue(x, 262.1);

export * from './import-attribute-3_FIXTURE.js'
with {};

export {} from './import-attribute-3_FIXTURE.js'
with {};
