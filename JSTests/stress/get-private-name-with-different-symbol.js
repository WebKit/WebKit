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

function classDeclaration() {
    class C {
        #field;

        setField(v) {
            this.#field = v;
        }

        getField() {
            return this.#field;
        }
    }

    noDFG(C.prototype.constructor);
    noFTL(C.prototype.constructor);
    noInline(C.prototype.setField);
    noDFG(C.prototype.setField);
    noFTL(C.prototype.setField);
    
    noInline(C.prototype.getField);

    return C;
}
noInline(classDeclaration);
let C1 = classDeclaration();
let C2 = classDeclaration();

for (let i = 0; i < 10000; i++) {
    let c = i % 2 ? new C1 : new C2;
    c.setField('test' + i);
    assert.equals(c.getField(), 'test' + i);
}
