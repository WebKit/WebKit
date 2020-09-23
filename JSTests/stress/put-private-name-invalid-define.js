//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true")

let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    },
    shouldThrow: function(exception, functor) {
        let threwException;
        try {
            functor();
            threwException = false;
        } catch(e) {
            threwException = true;
            if (!e instanceof exception)
                throw new Error("Expected to throw: " + exception.name + " but it throws: " + e.name);
        }

        if (!threwException)
            throw new Error("Expected to throw: " + exception.name + " but executed without exception");
    }
}

let shouldOverrideReturn = false;
let obj;

class B {
    constructor() {
        if (shouldOverrideReturn)
            return obj;
        return this;
    }
}

class C extends B {
  #field = 'test';

  getField() {
      return this.#field;
  }
}
noInline(C.constructor);
noInline(C.prototype.getField);
noDFG(C.prototype.getField);
noFTL(C.prototype.getField);

for (let i = 0; i < 10000; i++) {
  if (i > 1000) {
      shouldOverrideReturn = true;
      obj = {};
      let c = new C();
      assert.sameValue(C.prototype.getField.call(obj), 'test');
      assert.shouldThrow(TypeError, () => {
          new C();
      });
  } else {
      let obj = {};
      let c = new C();
      assert.sameValue(c.getField(), 'test');
      assert.shouldThrow(TypeError, () => {
          c.getField.call(obj);
      });
  }
}
