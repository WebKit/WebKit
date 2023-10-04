// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  `assert` AttributesKeyword in WithClause in ImportDeclaration may not
  be preceded by a line terminator
esid: sec-modules
info: |
  ImportDeclaration:
    import ModuleSpecifier[no LineTerminator here] WithClause;

  WithClause:
    AttributesKeyword {}
    AttributesKeyword { WithEntries ,opt }

  AttributesKeyword:
    with
    [no LineTerminator here] assert

  The restriction LineTerminator could be verified more simply with a negative
  syntax test. This test is designed to parse successfully in order to verify
  the restriction more precisely.
features: [import-assertions, globalThis]
flags: [module, raw]
---*/

var callCount = 0;

// Define a property on the global "this" value so that the effect of the
// expected IdentifierReference can be observed.
Object.defineProperty(globalThis, 'assert', {
  get: function() {
    callCount += 1;
  }
});

import * as x from './import-assertion-1_FIXTURE.js'
assert
{ type: 'json' };

if (x.default !== 262.1) {
  throw 'module value incorrectly imported - first declaration';
}

if (callCount !== 1) {
  throw 'IdentifierReference not recognized - first declaration';
}
