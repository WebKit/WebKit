function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

Object.defineProperty(globalThis.__proto__, "foo", {
    set(val) {
        throw new Error("The setter shouldn't be invoked!");
    }
});

var barSetterCalls = 0;
var barSetterValue;
Object.defineProperty(globalThis, "bar", {
    set(val) {
        barSetterCalls++;
        barSetterValue = val;
    }
});

eval(`if (true) {
    function foo() {}
    function bar() {}
}`);

assert(typeof foo === "function");
assert(barSetterCalls === 1);
assert(typeof barSetterValue === "function");
