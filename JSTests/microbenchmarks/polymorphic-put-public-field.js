class C {
    field;

    setField(value) {
        this.field = value;
    }
}
noInline(C.prototype.setField);

let c1 = new C();
c1.foo = 0;

let c2 = new C();
c2.bar = 0;

let c3 = new C();
c3.baz = 0;

let arr = [c1, c2, c3];

for (let i = 0; i < 5000000; i++) {
    arr[i % arr.length].setField(i);
}

