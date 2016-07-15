// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-runtime-semantics-classdefinitionevaluation
description: >
  The `this` value of a null-extending class is automatically initialized,
  preventing the use of `super` from within the constructor.
info: |
  Runtime Semantics: ClassDefinitionEvaluation

  [...]
  5. If ClassHeritageopt is not present, then
     [...]
  6. Else,
     [...]
     e. If superclass is null, then
        i. Let protoParent be null.
        ii. Let constructorParent be the intrinsic object %FunctionPrototype%.
  [...]
  15. If ClassHeritageopt is present and protoParent is not null, then set F's
      [[ConstructorKind]] internal slot to "derived".
  [...]

  9.2.2 [[Construct]]

  [...]
  5. If kind is "base", then
     a. Let thisArgument be ? OrdinaryCreateFromConstructor(newTarget,
        "%ObjectPrototype%").
  [...]

  12.3.5.1 Runtime Semantics: Evaluation

  SuperCall : super Arguments

  [...]
  6. Let result be ? Construct(func, argList, newTarget).
  7. Let thisER be GetThisEnvironment( ).
  8. Return ? thisER.BindThisValue(result).

  8.1.1.3.1 BindThisValue

  [...]
  3. If envRec.[[ThisBindingStatus]] is "initialized", throw a ReferenceError
     exception.
  4. Set envRec.[[ThisValue]] to V.
  5. Set envRec.[[ThisBindingStatus]] to "initialized".
  [...]
---*/

var unreachable = 0;
var reachable = 0;

class C extends null {
  constructor() {
    reachable += 1;
    super();
    unreachable += 1;
  }
}

assert.throws(TypeError, function() {
  new C();
});

assert.sameValue(reachable, 1);
assert.sameValue(unreachable, 0);
