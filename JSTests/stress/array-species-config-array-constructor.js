class A extends Array { }
Object.defineProperty(Array, Symbol.species, { value: A, configurable: true });

foo = [1,2,3,4];
result = foo.concat([1]);
if (!(result instanceof A))
    throw "concat failed";

result = foo.splice();
if (!(result instanceof A))
    throw "splice failed";

result = foo.slice();
if (!(result instanceof A))
    throw "slice failed";

Object.defineProperty(Array, Symbol.species, { value: Int32Array, configurable: true });

// We can't write to the length property on a typed array by default.
Object.defineProperty(Int32Array.prototype, "length", { value: 0, writable: true });

function shouldThrow(f, m) {
    let err;
    try {
        f();
    } catch(e) {
        err = e;
    }
    if (err.toString() !== m)
        throw new Error("Wrong error: " + err);
}

function test() {
    const message = "TypeError: Attempting to configure non-configurable property on a typed array at index: 0";
    shouldThrow(() => foo.concat([1]), message);
    foo = [1,2,3,4];
    shouldThrow(() => foo.slice(0), message);
    foo = [1,2,3,4];
    let r = foo.splice();
    if (!(r instanceof Int32Array))
        throw "Bad";
    if (r.length !== 0)
        throw "Bad";
    foo = [1,2,3,4];
    shouldThrow(() => foo.splice(0), message);
}
noInline(test);
for (let i = 0; i < 3000; ++i)
    test();
