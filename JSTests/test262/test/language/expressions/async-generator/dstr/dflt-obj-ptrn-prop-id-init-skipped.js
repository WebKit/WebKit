// This file was procedurally generated from the following sources:
// - src/dstr-binding/obj-ptrn-prop-id-init-skipped.case
// - src/dstr-binding/default/async-gen-func-expr-dflt.template
/*---
description: Destructuring initializer is not evaluated when value is not `undefined` (async generator function expression (default parameter))
esid: sec-asyncgenerator-definitions-evaluation
features: [async-iteration]
flags: [generated, async]
info: |
    AsyncGeneratorExpression : async [no LineTerminator here] function * ( FormalParameters ) {
        AsyncGeneratorBody }

        [...]
        3. Let closure be ! AsyncGeneratorFunctionCreate(Normal, FormalParameters,
           AsyncGeneratorBody, scope, strict).
        [...]


    13.3.3.7 Runtime Semantics: KeyedBindingInitialization

    BindingElement : BindingPattern Initializeropt

    [...]
    3. If Initializer is present and v is undefined, then
    [...]
---*/
var initCount = 0;
function counter() {
  initCount += 1;
}


var callCount = 0;
var f;
f = async function*({ s: t = counter(), u: v = counter(), w: x = counter(), y: z = counter() } = { s: null, u: 0, w: false, y: '' }) {
  assert.sameValue(t, null);
  assert.sameValue(v, 0);
  assert.sameValue(x, false);
  assert.sameValue(z, '');
  assert.sameValue(initCount, 0);

  assert.throws(ReferenceError, function() {
    s;
  });
  assert.throws(ReferenceError, function() {
    u;
  });
  assert.throws(ReferenceError, function() {
    w;
  });
  assert.throws(ReferenceError, function() {
    y;
  });
  callCount = callCount + 1;
};

f().next().then(() => {
    assert.sameValue(callCount, 1, 'invoked exactly once');
}).then($DONE, $DONE);
