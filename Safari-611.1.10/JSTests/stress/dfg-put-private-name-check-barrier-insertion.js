//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true", "--maxPolymorphicAccessInliningListSize=2")

let assert = {
    sameValue: function(a, e) {
        if (a !== e)
            throw new Error("Expected: " + e + " but got: " + a);
    },
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

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.setField);
noInline(C.prototype.getField);
noDFG(C.prototype.getField);
noFTL(C.prototype.getField);

let oldObject = new C;
for (; i < 10000; i++) {
    if (i < 5000) {
        let c = new C;
        c.setField(i);
    } else {
        if (i == 5000)
            gc();
        oldObject.setField({prop: i});
        edenGC();
        assert.sameValue(oldObject.getField().prop, i);
    }
}

