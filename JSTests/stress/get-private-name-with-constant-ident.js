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

let i = 0;

class C {
    #field = this.init();

    init() {
        if (i % 2)
            this.anotherField = i;
        return 'test';
    }

    setField(v) {
        this.#field = v;
    }

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.setField);
noInline(C.prototype.getField);
noDFG(C.prototype.setField);
noFTL(C.prototype.setField);

for (; i < 10000; i++) {
    count = i;
    let c = new C();
    assert.equals(c.getField(), 'test');
    c.setField('foo' + i);
    assert.equals(c.getField(), 'foo' + i);
}
