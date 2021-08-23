//@ requireOptions("--useAccessInlining=false")

let assert = {
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

class C {
  #field;

  setField(v) {
      let o = this;
      // This branch is here to avoid PutPrivateNameById to be folded as 
      // PutByOffset.
      if (v > 100)
          o = {};
      o.#field = v;
  }
}
noInline(C.constructor);
noInline(C.prototype.setField);

let c = new C();

// Let's trigger JIT compilation for `C.prototype.setField`
for (let i = 0; i < 10000; i++) {
    c.setField(15);
}

for (let i = 0; i < 10000; i++) {
    assert.shouldThrow(TypeError, () => {
        c.setField.call(15, 'test');
    });

    assert.shouldThrow(TypeError, () => {
        c.setField.call('string', 'test');
    });

    assert.shouldThrow(TypeError, () => {
        c.setField.call(Symbol('symbol'), 'test');
    });
}

