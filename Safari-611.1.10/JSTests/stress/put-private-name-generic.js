//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true")

let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    }
}

function classDeclaration(dynamicProperty) {
    class C {
        #field = this.init();
    
        init() {
            this[dynamicProperty] = dynamicProperty;
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

    return C;
}
noInline(classDeclaration);

for (let i = 0; i < 10000; i++) {
    let Class = classDeclaration('property' + i);
    let c = new Class();
    c.setField('test');
    assert.sameValue(c.getField(), 'test');
}

