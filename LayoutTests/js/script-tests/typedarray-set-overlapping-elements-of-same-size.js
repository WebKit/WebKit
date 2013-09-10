description(
"Tests the code path of typedArray.set that tries to do a memmove-with-conversion for overlapping arrays."
);

function foo(n) {
    var array = new Int32Array(n + 1);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    array.set(new Uint32Array(array.buffer, 0, n), 1);
    return array;
}

function bar(n) {
    var array = new Int32Array(n + 1);
    for (var i = 0; i < n; ++i)
        array[i + 1] = 42 + i;
    array.set(new Uint32Array(array.buffer, 4), 0);
    return array;
}

shouldBe("foo(10)", "[42,42,42,42,42,42,42,42,42,42,42]");
shouldBe("bar(10)", "[42,43,44,45,46,47,48,49,50,51,51]");

