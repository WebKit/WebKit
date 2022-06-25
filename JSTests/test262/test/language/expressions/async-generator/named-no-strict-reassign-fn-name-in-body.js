// This file was procedurally generated from the following sources:
// - src/function-forms/reassign-fn-name-in-body.case
// - src/function-forms/expr-named/async-gen-func-expr-named-no-strict.template
/*---
description: Reassignment of function name is silently ignored in non-strict mode code. (async generator named function expression in non-strict mode code)
esid: sec-asyncgenerator-definitions-evaluation
features: [async-iteration]
flags: [generated, async, noStrict]
info: |
    AsyncGeneratorExpression :
        async function * BindingIdentifier ( FormalParameters ) { AsyncGeneratorBody }

---*/

// increment callCount in case "body"
let callCount = 0;
let ref = async function * BindingIdentifier() {
  callCount++;
  BindingIdentifier = 1;
  return BindingIdentifier;
};

(async () => {
  assert.sameValue((await (await ref()).next()).value, ref);
  assert.sameValue(callCount, 1, 'function invoked exactly once');
})().then($DONE, $DONE);

