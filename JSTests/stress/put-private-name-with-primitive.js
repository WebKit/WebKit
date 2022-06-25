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
  #field = 'test';

  setField(v) {
      this.#field = v;
  }
}
noInline(C.constructor);
noInline(C.prototype.setField);

let c = new C();

// We repeat this to make operationPutPrivateNameOptimize repatch to operationPutPrivateNameGeneric
for (let i = 0; i < 5; i++) {
    assert.shouldThrow(TypeError, () => {
        c.setField.call(15, 0);
    });
    assert.shouldThrow(TypeError, () => {
        c.setField.call('test', 0);
    });
    assert.shouldThrow(TypeError, () => {
        c.setField.call(Symbol, 0);
    });
    assert.shouldThrow(TypeError, () => {
        c.setField.call(null, 0);
    });
    assert.shouldThrow(TypeError, () => {
        c.setField.call(undefined, 0);
    });
    assert.shouldThrow(TypeError, () => {
        c.setField.call(10n, 0);
    });
}

