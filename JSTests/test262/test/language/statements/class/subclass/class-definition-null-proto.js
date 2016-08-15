// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-runtime-semantics-classdefinitionevaluation
es6id: 14.5.14
description: >
  When a null-extending class does not specify a `constructor` method
  definition, a method with zero parameters and an empty body is used
info: |
  The behavior under test was introduced in the "ES2017" revision of the
  specification and conflicts with prior editions.

  Runtime Semantics: ClassDefinitionEvaluation

  5. If ClassHeritageopt is not present, then
     [...]
  6. Else,
     [...]
     e. If superclass is null, then
        i. Let protoParent be null.
        ii. Let constructorParent be the intrinsic object %FunctionPrototype%.
     [...]
  7. Let proto be ObjectCreate(protoParent).
  8. If ClassBodyopt is not present, let constructor be empty.
  9. Else, let constructor be ConstructorMethod of ClassBody.
  10. If constructor is empty, then
      a. If ClassHeritageopt is present and protoParent is not null, then
         [...]
      b. Else,
         i. Let constructor be the result of parsing the source text

              constructor( ){ }

            using the syntactic grammar with the goal symbol MethodDefinition.
  [...]
---*/

class Foo extends null {}

assert.sameValue(Object.getPrototypeOf(Foo.prototype), null);
assert.sameValue(Object.getPrototypeOf(Foo.prototype.constructor), Function.prototype);
