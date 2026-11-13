let assert = Object.assign(
  function assert(expr, message = "") {
    if (expr == false)
      throw new Error(`Expected ${message || "expr"} to be true, but was ${expr}`);
  }, {
  false(expr, message = "") {
    if (expr == true)
      throw new Error(`Expected ${message || "expr"} to be false, but was ${expr}`)
  },
  equals(a, b) {
    if (Object.is(a, b))
      return;

    throw new Error(`Expected ${a} to be ${b}`);
  }
});

// The base class is declared in eval specifically so that it can add enough
// properties to the class to store private fields in out-of-line property storage.
// JSFinalObject caps its inline capacity at 62 slots, so 70 properties is past it.
let Base = eval(`(class Base {
  // Methods
  constructor() {
    // Define properties
    ${Array.from({ length: 70 }, (_, i) => `  this.x${i} = ${i};\n`).join("")}
  }
})`);

// A private field cannot be added to a non-extensible object:
// https://github.com/tc39/proposal-nonextensible-applies-to-private
class PrivateFieldAfterPreventExtensions extends Base {
  #i = (Object.preventExtensions(this), 42);
}

class PrivateFieldBeforePreventExtensions extends Base {
  #i = 42;

  get() { return this.#i; }
  set(i) { this.#i = i; }
}

function testThrows() {
  try {
    new PrivateFieldAfterPreventExtensions;
  } catch (e) {
    assert(e instanceof TypeError, `${e} instanceof TypeError`);
    return;
  }

  throw new Error("Expected defining a private field on a non-extensible object to throw");
}
noInline(testThrows);

// preventExtensions leaves an existing private field writable, since it does not apply to private
// names.
function test(i) {
  let c = new PrivateFieldBeforePreventExtensions;
  assert(70 > $vm.inlineCapacity(c), "the private field is in out-of-line storage");
  assert.equals(c.x69, 69);
  c.x69 = 0.1;
  Object.preventExtensions(c);
  assert.false(Object.isExtensible(c), "Object.isExtensible(c)");
  assert.equals(c.get(), 42);
  c.set(i);
  assert.equals(c.get(), i);
}
noInline(test);

testThrows();
test(0);
test(1);
test(2);
for (var i = 0; i < 200; ++i) {
  testThrows();
  test(i);
}
