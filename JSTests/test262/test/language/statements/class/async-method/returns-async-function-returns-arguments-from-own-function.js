// This file was procedurally generated from the following sources:
// - src/async-functions/returns-async-function-returns-arguments-from-own-function.case
// - src/async-functions/evaluation/async-class-decl-method.template
/*---
description: Async function returns an async function. (Async method as a ClassDeclaration element)
esid: prod-AsyncMethod
features: [async-functions]
flags: [generated, async]
info: |
    ClassElement :
      MethodDefinition

    MethodDefinition :
      AsyncMethod

    Async Function Definitions

    AsyncMethod :
      async [no LineTerminator here] PropertyName ( UniqueFormalParameters ) { AsyncFunctionBody }

---*/
let count = 0;


class C {
  async method(x) {
    let a = arguments;
      return async function() { return a === arguments; };
  }
}
// Stores a reference `asyncFn` for case evaluation
let c = new C();
let asyncFn = c.method.bind(c);

asyncFn(1).then(retFn => {
  count++;
  return retFn();
}).then(result => {
  assert.sameValue(result, false);
  assert.sameValue(count, 1);
}).then($DONE, $DONE);
