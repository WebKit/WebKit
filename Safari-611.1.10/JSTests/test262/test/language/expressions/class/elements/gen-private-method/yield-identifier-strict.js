// This file was procedurally generated from the following sources:
// - src/generators/yield-identifier-strict.case
// - src/generators/default/class-expr-private-method.template
/*---
description: It's an early error if the generator body has another function body with yield as an identifier in strict mode. (Generator private method as a ClassExpression element)
esid: prod-GeneratorPrivateMethod
features: [generators, class-methods-private]
flags: [generated, onlyStrict]
negative:
  phase: parse
  type: SyntaxError
info: |
    ClassElement :
      PrivateMethodDefinition

    MethodDefinition :
      GeneratorMethod

    14.4 Generator Function Definitions

    GeneratorMethod :
      * PropertyName ( UniqueFormalParameters ) { GeneratorBody }

---*/
$DONOTEVALUATE();

var callCount = 0;

var C = class {
    *#gen() {
        callCount += 1;
        (function() {
            var yield;
            throw new Test262Error();
          }())
    }
    get gen() { return this.#gen; }
}

const c = new C();

// Test the private fields do not appear as properties before set to value
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#gen"), false, 'Object.hasOwnProperty.call(C.prototype, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(C, "#gen"), false, 'Object.hasOwnProperty.call(C, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(c, "#gen"), false, 'Object.hasOwnProperty.call(c, "#gen")');

var iter = c.gen();



assert.sameValue(callCount, 1);

// Test the private fields do not appear as properties after set to value
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#gen"), false, 'Object.hasOwnProperty.call(C.prototype, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(C, "#gen"), false, 'Object.hasOwnProperty.call(C, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(c, "#gen"), false, 'Object.hasOwnProperty.call(c, "#gen")');
