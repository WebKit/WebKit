// Copyright (C) 2019 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-generator-function-definitions-runtime-semantics-evaluation
description: >
  Return resumption value is awaited upon and hence is treated as a thenable.
info: |
  14.4.14 Runtime Semantics: Evaluation
    YieldExpression : yield* AssignmentExpression

    ...
    7. Repeat,
      a. If received.[[Type]] is normal, then
        i. Let innerResult be ? Call(iteratorRecord.[[NextMethod]], iteratorRecord.[[Iterator]],
                                     « received.[[Value]] »).
        ii. If generatorKind is async, then set innerResult to ? Await(innerResult).
        ...
        vi. If generatorKind is async, then set received to AsyncGeneratorYield(? IteratorValue(innerResult)).
        ...
      ...
      c. Else,
        i. Assert: received.[[Type]] is return.
        ii. Let return be ? GetMethod(iterator, "return").
        iii. If return is undefined, then
          1. If generatorKind is async, then set received.[[Value]] to ? Await(received.[[Value]]).
          2. Return Completion(received).
        ...

  25.5.3.7 AsyncGeneratorYield ( value )
    ...
    5. Set value to ? Await(value).
    ...
    8. Set the code evaluation state of genContext such that when evaluation is resumed with a
       Completion resumptionValue the following steps will be performed:
      ...
      b. Let awaited be Await(resumptionValue.[[Value]]).
      ...
      e. Return Completion { [[Type]]: return, [[Value]]: awaited.[[Value]], [[Target]]: empty }.
      ...

  6.2.3.1 Await
    ...
    2. Let promise be ? PromiseResolve(%Promise%, « value »).
    ...

  25.6.4.5.1 PromiseResolve ( C, x )
    ...
    3. Let promiseCapability be ? NewPromiseCapability(C).
    4. Perform ? Call(promiseCapability.[[Resolve]], undefined, « x »).
    ...

  25.6.1.5 NewPromiseCapability ( C )
    ...
    7. Let promise be ? Construct(C, « executor »).
    ...

  25.6.3.1 Promise ( executor )
    ...
    8. Let resolvingFunctions be CreateResolvingFunctions(promise).
    ...

  25.6.1.3 CreateResolvingFunctions ( promise )
    ...
    2. Let stepsResolve be the algorithm steps defined in Promise Resolve Functions (25.6.1.3.2).
    3. Let resolve be CreateBuiltinFunction(stepsResolve, « [[Promise]], [[AlreadyResolved]] »).
    ...

  25.6.1.3.2 Promise Resolve Functions
    ...
    9. Let then be Get(resolution, "then").
    ...

includes: [compareArray.js]
flags: [async]
features: [async-iteration]
---*/

var expected = [
  "start",

  // `Await(innerResult)` promise resolved.
  "tick 1",

  // `Await(value)` promise resolved.
  "tick 2",

  // "then" of `resumptionValue.[[Value]]` accessed.
  "get then",

  // `Await(resumptionValue.[[Value]])` promise resolved.
  "tick 3",

  // Get iterator "return" method.
  "get return",

  // "then" of `received.[[Value]]` accessed.
  "get then",

  // `Await(received.[[Value]])` promise resolved.
  "tick 4",
];

var actual = [];

var asyncIter = {
  [Symbol.asyncIterator]() {
    return this;
  },
  next() {
    return {
      done: false,
    };
  },
  get return() {
    actual.push("get return");
  }
};

async function* f() {
  actual.push("start");
  yield* asyncIter;
  actual.push("stop - never reached");
}

Promise.resolve(0)
  .then(() => actual.push("tick 1"))
  .then(() => actual.push("tick 2"))
  .then(() => actual.push("tick 3"))
  .then(() => actual.push("tick 4"))
  .then(() => {
    assert.compareArray(actual, expected, "Ticks for return with thenable getter");
}).then($DONE, $DONE);

var it = f();

// Start generator execution.
it.next();

// Stop generator execution.
it.return({
  get then() {
    actual.push("get then");
  }
});
