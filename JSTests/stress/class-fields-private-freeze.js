let assert = Object.assign(
  function assert(expr, message = "") {
    if (expr == false)
      throw new Error(`Expected ${message || "expr"} to be true, but was ${expr}`);
  }, {
  equals(a, b) {
    if (Object.is(a, b))
      return;

    throw new Error(`Expected ${a} to be ${b}`);
  }
});

class Base {
  constructor() {
    this.x = 1;
  }
}

// A private field cannot be added to a non-extensible object:
// https://github.com/tc39/proposal-nonextensible-applies-to-private
class PrivateFieldAfterFreeze extends Base {
  #i = (Object.freeze(this), 42);
}

class PrivateFieldBeforeFreeze extends Base {
  #i = 42;

  get() { return this.#i; }
  set(i) { this.#i = i; }
}

function testThrows() {
  try {
    new PrivateFieldAfterFreeze;
  } catch (e) {
    assert(e instanceof TypeError, `${e} instanceof TypeError`);
    return;
  }

  throw new Error("Expected defining a private field on a frozen object to throw");
}
noInline(testThrows);

// Freezing leaves an existing private field writable, since it does not apply to private names.
function test(i) {
  let c = new PrivateFieldBeforeFreeze;
  c.x = 0.1;
  Object.freeze(c);
  assert(Object.isFrozen(c), "Object.isFrozen(c)");
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
