function assert(b) {
    if (!b)
        throw new Error;
}

let result;
Object.defineProperty(Object.prototype, '1', {
    get() { return result; },
    set(x) { result = x; }
});
Array.prototype.length = 0x10000000;
Array.prototype[1] = 42;
assert(result === 42);
assert(Array.prototype[1] === 42);
