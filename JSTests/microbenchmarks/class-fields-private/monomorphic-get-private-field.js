//@ requireOptions("--usePrivateClassFields=true")

class C {
    #field;

    constructor(i) {
        this.#field = i;
    }

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.getField);

let c = new C("test");
for (let i = 0; i < 5000000; i++) {
    if (c.getField() !== "test")
        throw new Error("unexpected field value");
}
