function assert(a) {
    if (!a)
        throw new Error("Bad");
}

class A extends Function {
    t() {
        super.call(this);
        return 3;
    }
}

let a = new A();
assert(a.t() == 3);

