// This file was procedurally generated from the following sources:
// - src/async-generators/yield-identifier-strict.case
// - src/async-generators/default/async-class-expr-method.template
/*---
description: It's an early error if the generator body has another function body with yield as an identifier in strict mode. (Async generator method as a ClassExpression element)
esid: prod-AsyncGeneratorMethod
features: [async-iteration]
flags: [generated, onlyStrict]
negative:
  phase: parse
  type: SyntaxError
info: |
    ClassElement :
      MethodDefinition

    MethodDefinition :
      AsyncGeneratorMethod

    Async Generator Function Definitions

    AsyncGeneratorMethod :
      async [no LineTerminator here] * PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }

---*/
$DONOTEVALUATE();


var callCount = 0;

var C = class { async *gen() {
    callCount += 1;
    (function() {
        var yield;
        throw new Test262Error();
      }())
}}

var gen = C.prototype.gen;

var iter = gen();



assert.sameValue(callCount, 1);
