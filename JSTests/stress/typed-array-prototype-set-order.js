function assert(b) {
    if (!b)
        throw new Error;
}

function foo(n) {
    var array = new Uint8ClampedArray(n + 1);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    array.set(new Int8Array(array.buffer, 0, n), 1);
    return array;
}
assert(foo(10).toString() === "42,42,43,44,45,46,47,48,49,50,51");
