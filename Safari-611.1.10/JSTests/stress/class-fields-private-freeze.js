//@ requireOptions("--usePrivateClassFields=1")

let assert = Object.assign(
  function assert(expr, message = "") {
    if (expr == false)
      throw new Error(`Expected ${message || "expr"} to be true, but was ${expr}`);
  }, {
  equals(a, b) {
    if (a === b || (a === a && b === b) || (a !== a && b !== b))
      return;

    throw new Error(`Expected ${a} to be ${b}`);
  }
});

class Base {
  constructor() {
    this.x = 1;
  }
}

class PrivateFieldAfterPreventExtensions extends Base {
  #i = (Object.freeze(this), 42);
  #assert = (assert(Object.isFrozen(this), "Object.isFrozen(this)"), this.#i + 1);

  get() { return this.#i; }
  set(i) { this.#i = i; }
}

function test(i) {
  let c = new PrivateFieldAfterPreventExtensions;
  c.x = 0.1;
  assert.equals(c.get(), 42);
  c.set(i);
  assert.equals(c.get(), i);
}
noInline(test);

test(0);
test(1);
test(2);
for (var i = 0; i < 200; ++i)
  test(i);
