function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

globalThis.bar = 1;
Object.preventExtensions(globalThis);

eval(`{
    function foo() {}
    function bar() {}
}`);

assert(typeof foo === "undefined");
assert(typeof bar === "function");
