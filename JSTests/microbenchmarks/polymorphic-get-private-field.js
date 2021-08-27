class C {
    #field;

    setField(value) {
        this.#field = value;
    }

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.getField);

let c1 = new C();
c1.foo = 0;
c1.setField("a");

let c2 = new C();
c2.bar = 0;
c2.setField("b");

let c3 = new C();
c3.baz = 0;
c3.setField("c");

let arr = [c1, c2, c3];
let values = ["a", "b", "c"];
for (let i = 0; i < 5000000; i++) {
    if (arr[i % arr.length].getField() !== values[i % values.length])
        throw new Error("unexpected field value");
}

