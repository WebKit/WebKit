function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, expectedError) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}!`);
    }
    if (!errorThrown)
        throw new Error(`Didn't throw!`);
};

let objFooSetterCalls = 0;
let obj;

const foo = "fooValue";

obj = {
    get foo() { return "fooGetter"; },
    foo: "fooValue",
    set foo(_val) { objFooSetterCalls++; },
};

shouldBe(obj.foo, undefined);
obj.foo = 1;
shouldBe(obj.foo, undefined);
shouldBe(objFooSetterCalls, 1);


obj = {
    set foo(_val) { objFooSetterCalls++; },
    foo,
    get foo() { return "fooGetter"; },
};

shouldBe(obj.foo, "fooGetter");
obj.foo = 1;
shouldThrow(() => { "use strict"; obj.foo = 2; }, "TypeError: Attempted to assign to readonly property.");
shouldBe(obj.foo, "fooGetter");
shouldBe(objFooSetterCalls, 1);


obj = {
    foo: "fooValue",
    get foo() { return "fooGetter"; },
};

shouldBe(obj.foo, "fooGetter");
obj.foo = 1;
shouldThrow(() => { "use strict"; obj.foo = 2; }, "TypeError: Attempted to assign to readonly property.");
shouldBe(obj.foo, "fooGetter");
shouldBe(objFooSetterCalls, 1);


obj = {
    foo,
    set foo(_val) { objFooSetterCalls++; },
};

shouldBe(obj.foo, undefined);
obj.foo = 1;
shouldBe(obj.foo, undefined);
shouldBe(objFooSetterCalls, 2);


obj = {
    foo: "fooValue",
    get foo() { return "fooGetter"; },
    set foo(_val) { objFooSetterCalls++; },
};

shouldBe(obj.foo, "fooGetter");
obj.foo = 1;
shouldBe(obj.foo, "fooGetter");
shouldBe(objFooSetterCalls, 3);


obj = {
    get foo() { return "fooGetter"; },
    set foo(_val) { objFooSetterCalls++; },
    foo,
};

shouldBe(obj.foo, "fooValue");
obj.foo = 1;
shouldBe(obj.foo, 1);
shouldBe(objFooSetterCalls, 3);
