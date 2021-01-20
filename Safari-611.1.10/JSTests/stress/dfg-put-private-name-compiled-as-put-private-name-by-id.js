//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true", "--maxPolymorphicAccessInliningListSize=2")

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

let i = 0;

class C {
    #field = this.init();

    init() {
        let arr = ["p1", "p2", "p3"];

        let key = arr[i % 3];
        this[key] = i;
        if (key === "p2")
            this["p1"] = i;

        if (key === "p3") {
            this["p1"] = i;
            this["p2"] = i;
        }
    }

    setField(v) {
        this.#field = v;
    }
}
noInline(C.prototype.setField);

for (; i < 10000; i++) {
    let c = new C;
    if (i > 5000) {
        assert.throws(TypeError, function() {
            c.setField.call({});
        });
    } else
        c.setField(i);
}

