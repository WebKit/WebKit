// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-runtime-semantics-classdefinitionevaluation
description: >
  The `this` value of a null-extending class is automatically initialized,
  obviating the need for an explicit return value in the constructor.
info: |
  The behavior under test was introduced in the "ES2017" revision of the
  specification and conflicts with prior editions.

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
  15. Return ? envRec.GetThisBinding().
---*/

class Foo extends null {
  constructor() {}
}

var foo = new Foo();

assert.sameValue(Object.getPrototypeOf(foo), Foo);
