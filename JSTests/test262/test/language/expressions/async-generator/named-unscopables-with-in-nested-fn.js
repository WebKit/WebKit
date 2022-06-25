// This file was procedurally generated from the following sources:
// - src/function-forms/unscopables-with-in-nested-fn.case
// - src/function-forms/default/async-gen-named-func-expr.template
/*---
description: Symbol.unscopables behavior across scope boundaries (async generator named function expression)
esid: sec-asyncgenerator-definitions-evaluation
features: [globalThis, Symbol.unscopables, async-iteration]
flags: [generated, noStrict, async]
info: |
    AsyncGeneratorExpression : async [no LineTerminator here] function * BindingIdentifier
        ( FormalParameters ) { AsyncGeneratorBody }

        [...]
        7. Let closure be ! AsyncGeneratorFunctionCreate(Normal, FormalParameters,
           AsyncGeneratorBody, funcEnv, strict).
        [...]


    ...
    Let envRec be lex's EnvironmentRecord.
    Let exists be ? envRec.HasBinding(name).

    HasBinding

    ...
    If the withEnvironment flag of envRec is false, return true.
    Let unscopables be ? Get(bindings, @@unscopables).
    If Type(unscopables) is Object, then
       Let blocked be ToBoolean(? Get(unscopables, N)).
       If blocked is true, return false.

    (The `with` Statement) Runtime Semantics: Evaluation

    ...
    Set the withEnvironment flag of newEnv’s EnvironmentRecord to true.
    ...

---*/
let count = 0;
var v = 1;
globalThis[Symbol.unscopables] = {
  v: true,
};

{
  count++;


var callCount = 0;
// Stores a reference `ref` for case evaluation
var ref;
ref = async function* g(x) {
  (function() {
    count++;
    with (globalThis) {
      count++;
      assert.sameValue(v, 1, 'The value of `v` is 1');
    }
  })();
  (function() {
    count++;
    var v = x;
    with (globalThis) {
      count++;
      assert.sameValue(v, 10, 'The value of `v` is 10');
      v = 20;
    }
    assert.sameValue(v, 20, 'The value of `v` is 20');
    assert.sameValue(globalThis.v, 1, 'The value of globalThis.v is 1');
  })();
  assert.sameValue(v, 1, 'The value of `v` is 1');
  assert.sameValue(globalThis.v, 1, 'The value of globalThis.v is 1');
  callCount = callCount + 1;
};

ref(10).next().then(() => {
    assert.sameValue(callCount, 1, 'generator function invoked exactly once');
}).then($DONE, $DONE);

  count++;
}
assert.sameValue(count, 6, 'The value of `count` is 6');
