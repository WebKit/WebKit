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
  #method() {}

  access(v) {
      let o = this;
      // This branch is here to avoid CheckPrivateBrand to be folded.
      if (v > 100)
          o = {};
      o.#method();
  }
}
noInline(C.prototype.access);

let c = new C();

// Let's trigger JIT compilation for `C.prototype.access`
for (let i = 0; i < 10000; i++) {
    c.access(15);
}

for (let i = 0; i < 10000; i++) {
    assert.shouldThrow(TypeError, () => {
        c.access.call({}, 0);
    });

    assert.shouldThrow(TypeError, () => {
        c.access.call(15, 0);
    });

    assert.shouldThrow(TypeError, () => {
        c.setField.call('string', 0);
    });

    assert.shouldThrow(TypeError, () => {
        c.setField.call(Symbol('symbol'), 0);
    });
}

