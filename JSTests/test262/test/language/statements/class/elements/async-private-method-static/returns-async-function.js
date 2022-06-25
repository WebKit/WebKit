// This file was procedurally generated from the following sources:
// - src/async-functions/returns-async-function.case
// - src/async-functions/evaluation/async-class-decl-static-private-method.template
/*---
description: Async function returns an async function. (Static async private method as a ClassDeclaration element)
esid: prod-AsyncMethod
features: [async-functions, class-static-methods-private]
flags: [generated, async]
info: |
    ClassElement :
      static PrivateMethodDefinition

    MethodDefinition :
      AsyncMethod

    Async Function Definitions

    AsyncMethod :
      async [no LineTerminator here] # PropertyName ( UniqueFormalParameters ) { AsyncFunctionBody }

---*/
let count = 0;


class C {
  static async #method(x) {
    return async function() { return x; };
  }
  static async method(x) {
    return this.#method(x);
  }
}
// Stores a reference `asyncFn` for case evaluation
let asyncFn = C.method.bind(C);

asyncFn(1).then(retFn => {
  count++;
  return retFn();
}).then(result => {
  assert.sameValue(result, 1);
  assert.sameValue(count, 1);
}).then($DONE, $DONE);
