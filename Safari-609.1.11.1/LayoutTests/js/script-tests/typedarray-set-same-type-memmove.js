description(
"Tests the code path of typedArray.set that bottoms out in memmove."
);

function foo(n) {
    var array = new Int32Array(n + 1);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    array.set(array.subarray(0, n), 1);
    return array;
}

function bar(n) {
    var array = new Int32Array(n + 1);
    for (var i = 0; i < n; ++i)
        array[i + 1] = 42 + i;
    array.set(array.subarray(1, n + 1), 0);
    return array;
}

function arraysEqual(a, b) {
    if (a.length != b.length)
        return false;
}

shouldBe("foo(10)", "[42,42,43,44,45,46,47,48,49,50,51]");
shouldBe("bar(10)", "[42,43,44,45,46,47,48,49,50,51,51]");
