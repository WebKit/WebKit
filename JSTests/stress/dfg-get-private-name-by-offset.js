// With no incompatible structure variants, GetPrivateName is converted to GetByOffset
// during bytecode parsing. We test that it's still semantically valid.

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
  #private = "private";
  getPrivate(o) { return o.#private; }
  constructor(i) {
    if (i === 995)
      return {};
  }
}
noInline(C.constructor);
noDFG(C.constructor);
noFTL(C.constructor);
noFTL(C.prototype.getPrivate);

let getPrivate = C.prototype.getPrivate;

function test(o) {
  return getPrivate(o);
}
neverInlineFunction(test);

// Warmup
test(new C, 0);
test(new C, 0);
test(new C, 0);

var i;
for (var j = 0; j < 2; ++j) {
  assert.throws(() => {
    for (i = 0; i < 2000; ++i) {
      assert.equals(test(new C(i)), "private");
      optimizeNextInvocation(test);
    }
  }, TypeError);

  // We should throw on iteration 995, because this will result in an invalid load.
  assert.equals(i, 995);
}
