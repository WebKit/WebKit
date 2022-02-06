function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

let objFooSetterCalls = 0;
let obj;

obj = class {
    static get foo() { return "fooGetter"; }
    static foo = "fooValue"
    static set foo(_val) { objFooSetterCalls++; }
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 0);


obj = class {
    static set foo(_val) { objFooSetterCalls++; }
    static foo = "fooValue"
    static get foo() { return "fooGetter"; }
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 0);


obj = class {
    static foo = "fooValue";
    static get foo() { return "fooGetter"; }
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 0);


obj = class {
    static foo = "fooValue"
    static set foo(_val) { objFooSetterCalls++; }
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 0);


obj = class {
    static get foo() { return "fooGetter"; }
    static set foo(_val) { objFooSetterCalls++; }
    static foo = "fooValue"
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 0);


obj = class {
    static foo = "fooValue"
    static get foo() { return "fooGetter"; }
    static set foo(_val) { objFooSetterCalls++; }
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 0);
