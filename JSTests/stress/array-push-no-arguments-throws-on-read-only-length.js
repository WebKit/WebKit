// array.push() with no arguments still sets "length", which throws when length is not writable.

function push(array) { return array.push(); }
noInline(push);

function make(i) {
    if (i % 3 == 0)
        return Object.freeze(["a", "b"]);
    if (i % 3 == 1) {
        const array = [1, 2];
        Object.defineProperty(array, "length", { writable: false });
        return array;
    }
    return ["x", "y"];
}

for (let i = 0; i < testLoopCount; ++i) {
    let result;
    try {
        result = push(make(i));
    } catch (e) {
        result = e.constructor.name;
    }
    const expected = i % 3 == 2 ? 2 : "TypeError";
    if (result !== expected)
        throw new Error("iteration " + i + ": expected " + expected + " but got " + result);
}
