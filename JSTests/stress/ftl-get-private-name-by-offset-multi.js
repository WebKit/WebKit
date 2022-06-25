// With many incompatible structure variants, this test will fail to generate GetByOffset dwuring DFGByteCodeParsing.
// During FTL compilation, it will try again in DFGConstantFoldingPhase.
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

let bases = [
  {
  },
  {
    a: 1,
  },
  {
    b: 1,
    a: 2,
  },
  {
    c: 1,
    d: 2,
    a: 3,
  },
  {
  },
  {
    z: 1,
    a: 2,
    b: 3,
    q: 4,
  },
  {
    a: 1,
    d: 2,
    z: 3,
    x: 4,
    f: 5,
  },
];
class Base {
  constructor(i) {
    if (i < 100)
      return Object.assign({}, bases[i & 3]);
    return Object.assign({}, bases[i % bases.length]);
  }
}
class C extends Base {
  #private = "private";
  getPrivate() { return this.#private; }
}
noInline(C.constructor);
noDFG(C.constructor);
noFTL(C.constructor);
noInline(C.prototype.getPrivate);

let getPrivate = C.prototype.getPrivate;

function test(o) {
  assert.equals(getPrivate.call(o), "private");
}

test(new C(0), 0);
test(new C(0), 0);
test(new C(0), 0);
for (var i = 0; !isFinalTier(test) && i < 10000; ++i) {
  test(new C(i));
  optimizeNextInvocation(test);
}
assert.throws(() => {
  test({})
});
