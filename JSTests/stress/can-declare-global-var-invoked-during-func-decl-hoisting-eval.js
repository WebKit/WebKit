function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

let barSetterValue;
Object.defineProperty(globalThis, "bar", { get() {}, set(val) { barSetterValue = val; }, enumerable: true, configurable: false });
Object.preventExtensions(globalThis);

eval(`{
    function foo() {}
    function bar() {}
}`);

assert(typeof foo === "undefined");
assert(typeof barSetterValue === "function");
assert(!globalThis.hasOwnProperty("foo"));
