//@requireOptions("--usePrivateClassFields=true", "--useLLInt=false", "--forceICFailure=true")
// Regression test: Ensure that we don't crash when op_get_private_field caching results in
// giving up on caching.`

function assert(expr, message) {
  if (!expr)
    throw new Error(`Assertion Failed: ${message}`);
}
Object.assign(assert, {
  equals(actual, expected) {
    assert(actual === expected, `expected ${expected} but found ${actual}`);
  },
  throws(fn, errorType) {
    try {
      fn();
    } catch (e) {
      if (typeof errorType === "function")
        assert(e instanceof errorType, `expected to throw ${errorType.name} but threw ${e}`);
      return;
    }
    assert(false, `expected to throw, but no exception was thrown.`);
  }
});

class C {
  #x = 5;
  get(o) { return o.#x; }
}
let get = C.prototype.get;
function testAccess() {
  assert.equals(get(new C), 5);
}
noInline(testAccess);
function testThrows() {
  assert.throws(() => get(globalThis), TypeError);
}
noInline(testThrows);

for (var i = 0; i < 20; ++i) {
  testAccess();
  testThrows();
}
