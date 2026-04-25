function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

const foo = {x: 0};
foo.__proto__ = new Proxy({y: 1}, { ownKeys() { return ['y']; } });
const keys = [];
for (const x in foo) {
    keys.push(x);
}

assert(keys.length == 2, "Should have 2 keys");
assert(keys.includes("x"), "Should have key `x`");
assert(keys.includes("y"), "Should have key `y`");
