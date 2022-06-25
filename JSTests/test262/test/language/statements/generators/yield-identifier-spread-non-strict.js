// This file was procedurally generated from the following sources:
// - src/generators/yield-identifier-spread-non-strict.case
// - src/generators/non-strict/declaration.template
/*---
description: Mixed use of object spread and yield as a valid identifier in a function body inside a generator body in non strict mode (Generator function declaration - valid for non-strict only cases)
esid: prod-GeneratorDeclaration
features: [Symbol, object-spread, generators]
flags: [generated, noStrict]
info: |
    14.4 Generator Function Definitions

    GeneratorDeclaration[Yield, Await, Default]:
      function * BindingIdentifier[?Yield, ?Await] ( FormalParameters[+Yield, ~Await] ) { GeneratorBody }


    Spread Properties

    PropertyDefinition[Yield]:
      (...)
      ...AssignmentExpression[In, ?Yield]

---*/
var s = Symbol('s');

var callCount = 0;

function *gen() {
  callCount += 1;
  yield {
       ...yield yield,
       ...(function(arg) {
          var yield = arg;
          return {...yield};
       }(yield)),
       ...yield,
    }
}

var iter = gen();

var iter = gen();

iter.next();
iter.next();
iter.next({ x: 10, a: 0, b: 0, [s]: 1 });
iter.next({ y: 20, a: 1, b: 1, [s]: 42 });
var item = iter.next({ z: 30, b: 2 });

var value = item.value;

assert.sameValue(item.done, false);
assert.sameValue(value.x, 10);
assert.sameValue(value.y, 20);
assert.sameValue(value.z, 30);
assert.sameValue(value.a, 1);
assert.sameValue(value.b, 2);
assert.sameValue(value[s], 42);
assert(Object.hasOwnProperty.call(value, s));
assert.sameValue(Object.keys(value).length, 5);

assert.sameValue(callCount, 1);
