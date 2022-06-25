// This file was procedurally generated from the following sources:
// - src/async-generators/yield-star-getiter-async-undefined-sync-get-abrupt.case
// - src/async-generators/default/async-class-decl-static-method.template
/*---
description: Abrupt completion while getting @@iterator after undefined @@asyncIterator (Static async generator method as a ClassDeclaration element)
esid: prod-AsyncGeneratorMethod
features: [Symbol.iterator, Symbol.asyncIterator, async-iteration]
flags: [generated, async]
info: |
    ClassElement :
      static MethodDefinition

    MethodDefinition :
      AsyncGeneratorMethod

    Async Generator Function Definitions

    AsyncGeneratorMethod :
      async [no LineTerminator here] * PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }


    YieldExpression: yield * AssignmentExpression

    1. Let exprRef be the result of evaluating AssignmentExpression.
    2. Let value be ? GetValue(exprRef).
    3. Let generatorKind be ! GetGeneratorKind().
    4. Let iterator be ? GetIterator(value, generatorKind).
    ...

    GetIterator ( obj [ , hint ] )

    ...
    3. If hint is async,
      a. Set method to ? GetMethod(obj, @@asyncIterator).
      b. If method is undefined,
        i. Let syncMethod be ? GetMethod(obj, @@iterator).
    ...

    GetMethod ( V, P )

    ...
    2. Let func be ? GetV(V, P).
    ...

---*/
var calls = 0;
var reason = {};
var obj = {
  get [Symbol.iterator]() {
    throw reason;
  },
  get [Symbol.asyncIterator]() {
    calls += 1;
    return undefined;
  }
};



var callCount = 0;

class C { static async *gen() {
    callCount += 1;
    yield* obj;
      throw new Test262Error('abrupt completion closes iter');

}}

var gen = C.gen;

var iter = gen();

iter.next().then(() => {
  throw new Test262Error('Promise incorrectly fulfilled.');
}, v => {
  assert.sameValue(v, reason, 'reject reason');
  assert.sameValue(calls, 1);

  iter.next().then(({ done, value }) => {
    assert.sameValue(done, true, 'the iterator is completed');
    assert.sameValue(value, undefined, 'value is undefined');
  }).then($DONE, $DONE);
}).catch($DONE);

assert.sameValue(callCount, 1);
