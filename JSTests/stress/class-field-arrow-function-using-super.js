function assert(actual, expected) {
    if (actual !== expected)
        throw Error("Expected: " + expected + " Actual: " + actual);
}

class C {
    func = () => {
        super.prop = "foo";
        return this.prop;
    };
}

let c = new C;
assert(c.func(), "foo");

