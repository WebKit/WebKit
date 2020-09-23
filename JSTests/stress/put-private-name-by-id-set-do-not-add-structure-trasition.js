//@requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true", "--useAccessInlining=false")

let assert = {
    throws: function(exception, functor) {
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

class Base {
  constructor() {
    return {};
  }
}

class C extends Base {
    #field = 0;

    getField(changeReciever) {
        let r = this;
        if (changeReciever) {
            r = {};
        }

        r.#field = 'bar';
    }
}
noInline(C.prototype.getField);

let c = new C();
for (let i = 0; i < 10000; i++) {
    if (i < 9000)
        C.prototype.getField.call(c, false);
    else {
        assert.throws(TypeError, function () {
            C.prototype.getField.call(c, true);
        });
    }
}

