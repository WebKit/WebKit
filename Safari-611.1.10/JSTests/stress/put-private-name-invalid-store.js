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

let shouldStore = false;

function foo() {
  class C {
    #field = this.init();
    #foo;

    init() {
      if (shouldStore)
        this.#foo = 12;
    }
  }

  return new C();
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
  if (i > 1000) {
    shouldStore = true;
    assert.shouldThrow(TypeError, () => {
      foo();
    });
  } else {
    foo();
  }
}
