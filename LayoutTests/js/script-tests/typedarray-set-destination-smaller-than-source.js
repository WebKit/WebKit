description(
"Tests the code path in typedArray.set that may have to do a copy via an intermediate buffer because the source and destination overlap and have different size elements (source is larger than destination)."
);

function foo_reference(n) {
    var array = new Int8Array(n);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    var array2 = new Int8Array(array);
    array2.set(new Int32Array(array.buffer));
    return array2;
}

function foo(n) {
    var array = new Int8Array(n);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    array.set(new Int32Array(array.buffer));
    return array;
}

function bar_reference(n) {
    var array = new Int8Array(n);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    var array2 = new Int8Array(array);
    array2.set(new Int32Array(array.buffer), n - n / 4);
    return array2;
}

function bar(n) {
    var array = new Int8Array(n);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    array.set(new Int32Array(array.buffer), n - n / 4);
    return array;
}

function baz_reference(n) {
    var array = new Int8Array(n);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    var array2 = new Int8Array(array);
    array2.set(new Int32Array(array.buffer), n / 2 - (n / 4) / 2);
    return array2;
}

function baz(n) {
    var array = new Int8Array(n);
    for (var i = 0; i < n; ++i)
        array[i] = 42 + i;
    array.set(new Int32Array(array.buffer), n / 2 - (n / 4) / 2);
    return array;
}

shouldBe("foo(64)", "foo_reference(64)");
shouldBe("bar(64)", "bar_reference(64)");
shouldBe("baz(64)", "baz_reference(64)");
