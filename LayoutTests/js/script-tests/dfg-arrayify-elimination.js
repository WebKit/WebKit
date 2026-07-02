function foo(a, i, j, k) {
    a[i] = 42;
    a[j] = 43;
    a[k] = 44;
}

for (var i = 0; i < 1000; ++i) {
    var array = [];
    if (i % 2)
        array.unshift(52);
    foo(array, 1, 2, 3);
    if (i % 2)
        shouldBe("array", "[52,42,43,44]");
    else
        shouldBe("array", "[,42,43,44]");
}
