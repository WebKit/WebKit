//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true", "--usePolymorphicAccessInlining=false")

let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    }
}

let iterations = 100000;
let triggerCompilation = false;
let constructDifferent = false;
class C {
    #field = this.init();

    init() {
        if (constructDifferent)
            this.foo = 0;
        return 0;
    }

    method(j, other) {
        let c = 0;
        let obj = this;
        for (let i = 0; triggerCompilation && i < iterations; i++)
            c++;
        if (j % 2) {
            other.foo = j;
            obj = other;
        }
        obj.#field = c;
    }

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.method);
noInline(C.prototype.getField);
noDFG(C.prototype.getField);
noFTL(C.prototype.getField);

let otherC = new C();

constructDifferent = true;

for (let i = 0; i < 30; i++) {
    let c = new C();
    c.method(i, otherC);
    assert.sameValue(c.getField(), 0);
}

triggerCompilation = true
let c = new C();
c.method(0, otherC);
assert.sameValue(c.getField(), iterations);

