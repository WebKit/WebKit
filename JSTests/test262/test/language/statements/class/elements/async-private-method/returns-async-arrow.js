// This file was procedurally generated from the following sources:
// - src/async-functions/returns-async-arrow.case
// - src/async-functions/evaluation/async-class-decl-private-method.template
/*---
description: Async function returns an async function. (Async private method as a ClassDeclaration element)
esid: prod-AsyncMethod
features: [async-functions, class-methods-private]
flags: [generated, async]
info: |
    ClassElement :
      PrivateMethodDefinition

    MethodDefinition :
      AsyncMethod

    Async Function Definitions

    AsyncMethod :
      async [no LineTerminator here] # PropertyName ( UniqueFormalParameters ) { AsyncFunctionBody }

---*/
let count = 0;


class C {
  async #method(x) {
    return async () => x;
  }
  async method(x) {
    return this.#method(x);
  }
}
// Stores a reference `asyncFn` for case evaluation
let c = new C();
let asyncFn = c.method.bind(c);

asyncFn(1).then(retFn => {
  count++;
  return retFn();
}).then(result => {
  assert.sameValue(result, 1);
  assert.sameValue(count, 1);
}).then($DONE, $DONE);
