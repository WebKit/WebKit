// This file was procedurally generated from the following sources:
// - src/function-forms/reassign-fn-name-in-body-in-eval.case
// - src/function-forms/expr-named/async-func-expr-named-no-strict.template
/*---
description: Reassignment of function name is silently ignored in non-strict mode code. (async function named expression in non-strict mode code)
esid: sec-async-function-definitions
features: [async-functions]
flags: [generated, async, noStrict]
info: |
    Async Function Definitions

    AsyncFunctionExpression :
      async function BindingIdentifier ( FormalParameters ) { AsyncFunctionBody }

---*/

// increment callCount in case "body"
let callCount = 0;
let ref = async function BindingIdentifier() {
  callCount++;
  eval("BindingIdentifier = 1");
  return BindingIdentifier;
};

(async () => {
  assert.sameValue(await ref(), ref);
  assert.sameValue(callCount, 1, 'function invoked exactly once');
})().then($DONE, $DONE);

