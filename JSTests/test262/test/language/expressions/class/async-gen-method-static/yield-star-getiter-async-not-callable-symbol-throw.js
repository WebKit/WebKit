// This file was procedurally generated from the following sources:
// - src/async-generators/yield-star-getiter-async-not-callable-symbol-throw.case
// - src/async-generators/default/async-class-expr-static-method.template
/*---
description: Throws a TypeError on a non-callable [Symbol.asyncIterator] (symbol) (Static async generator method as a ClassExpression element)
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
    ...

    GetMethod ( V, P )

    ...
    2. Let func be ? GetV(V, P).
    3. If func is either undefined or null, return undefined.
    4. If IsCallable(func) is false, throw a TypeError exception.
    ...

---*/
var obj = {
  get [Symbol.iterator]() {
    throw new Test262Error('it should not get Symbol.iterator');
  },
  [Symbol.asyncIterator]: Symbol.asyncIterator
};



var callCount = 0;

var C = class { static async *gen() {
    callCount += 1;
    yield* obj;
      throw new Test262Error('abrupt completion closes iter');

}}

var gen = C.gen;

var iter = gen();

iter.next().then(() => {
  throw new Test262Error('Promise incorrectly fulfilled.');
}, v => {
  assert.sameValue(v.constructor, TypeError, "TypeError");

  iter.next().then(({ done, value }) => {
    assert.sameValue(done, true, 'the iterator is completed');
    assert.sameValue(value, undefined, 'value is undefined');
  }).then($DONE, $DONE);
}).catch($DONE);

assert.sameValue(callCount, 1);
