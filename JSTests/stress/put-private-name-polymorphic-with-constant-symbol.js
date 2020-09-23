//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true")

let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    }
}

let i = 0;

class C {
    #field = this.init();

    init() {
        if (i % 2)
            this.anotherField = i;
        return 'test';
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

for (; i < 10000; i++) {
    count = i;
    let c = new C();
    assert.sameValue(c.getField(), 'test');
    c.setField('foo' + i);
    assert.sameValue(c.getField(), 'foo' + i);
}

