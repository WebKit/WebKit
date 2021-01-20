//@ requireOptions("--allowUnsupportedTiers=true", "--usePrivateClassFields=true")

class C {
    #field;

    setField(value) {
        this.#field = value;
    }
}
noInline(C.prototype.setField);

let c = new C();
for (let i = 0; i < 5000000; i++) {
    c.setField(i);
}

