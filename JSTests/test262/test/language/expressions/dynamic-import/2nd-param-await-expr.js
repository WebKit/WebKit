// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  ImportCall parameter list forwards the Await production parameter - AwaitExpression
esid: sec-import-call-runtime-semantics-evaluation
info: |
  ImportCall[Yield, Await]:
    import ( AssignmentExpression[+In, ?Yield, ?Await] ,opt )
    import ( AssignmentExpression[+In, ?Yield, ?Await] , AssignmentExpression[+In, ?Yield, ?Await] ,opt )
features: [dynamic-import, import-assertions, async-functions]
flags: [async]
---*/

(async function () {
  return import('./2nd-param_FIXTURE.js', await undefined);
}())
  .then(function(module) {
    assert.sameValue(module.default, 262);
  })
  .then($DONE, $DONE);
