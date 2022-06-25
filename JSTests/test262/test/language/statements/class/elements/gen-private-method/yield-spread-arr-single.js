// This file was procedurally generated from the following sources:
// - src/generators/yield-spread-arr-single.case
// - src/generators/default/class-decl-private-method.template
/*---
description: Use yield value in a array spread position (Generator private method as a ClassDeclaration element)
esid: prod-GeneratorPrivateMethod
features: [generators, class-methods-private]
flags: [generated]
info: |
    ClassElement :
      PrivateMethodDefinition

    MethodDefinition :
      GeneratorMethod

    14.4 Generator Function Definitions

    GeneratorMethod :
      * PropertyName ( UniqueFormalParameters ) { GeneratorBody }


    Array Initializer

    SpreadElement[Yield, Await]:
      ...AssignmentExpression[+In, ?Yield, ?Await]
---*/
var arr = ['a', 'b', 'c'];

var callCount = 0;

class C {
    *#gen() {
        callCount += 1;
        yield [...yield];
    }
    get gen() { return this.#gen; }
}

const c = new C();

// Test the private fields do not appear as properties before set to value
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#gen"), false, 'Object.hasOwnProperty.call(C.prototype, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(C, "#gen"), false, 'Object.hasOwnProperty.call(C, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(c, "#gen"), false, 'Object.hasOwnProperty.call(c, "#gen")');

var iter = c.gen();

iter.next(false);
var item = iter.next(arr);
var value = item.value;

assert.notSameValue(value, arr, 'value is a new array');
assert(Array.isArray(value), 'value is an Array exotic object');
assert.sameValue(value.length, 3)
assert.sameValue(value[0], 'a');
assert.sameValue(value[1], 'b');
assert.sameValue(value[2], 'c');
assert.sameValue(item.done, false);

assert.sameValue(callCount, 1);

// Test the private fields do not appear as properties after set to value
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#gen"), false, 'Object.hasOwnProperty.call(C.prototype, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(C, "#gen"), false, 'Object.hasOwnProperty.call(C, "#gen")');
assert.sameValue(Object.hasOwnProperty.call(c, "#gen"), false, 'Object.hasOwnProperty.call(c, "#gen")');
