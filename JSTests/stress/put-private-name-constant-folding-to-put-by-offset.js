//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true", "--useAccessInlining=false")

let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    }
}

let iterations = 10000;
let triggerCompilation = false;
class C {
    #field = 0;

    method() {
        let c = 0;
        for (let i = 0; triggerCompilation && i < iterations; i++)
            c++;
        this.#field = c;
    }

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.getField);
noDFG(C.prototype.getField);
noFTL(C.prototype.getField);

let c = new C();
for (let i = 0; i < 100; i++) {
    c.method();
    assert.sameValue(c.getField(), 0);
}

triggerCompilation = true
c.method();
assert.sameValue(c.getField(), iterations);

