//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true")

let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    }
}

function classDeclaration() {
    class C {
        #field;
    
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

    return C;
}
noInline(classDeclaration);
let C1 = classDeclaration();
let C2 = classDeclaration();

for (let i = 0; i < 10000; i++) {
    let c = i % 2 ? new C1 : new C2;
    c.setField('test' + i);
    assert.sameValue(c.getField(), 'test' + i);
}

