function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let source = new Array(6);
    source[0] = 42;
    let result = source.toReversed();
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 5)
            shouldBe(result[i], 42);
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = 42.195;
    let result = source.toReversed();
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 5)
            shouldBe(result[i], 42.195);
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = "string";
    let result = source.toReversed();
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 5)
            shouldBe(result[i], "string");
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = "string";
    $vm.ensureArrayStorage(source);
    source[5] = "Hello";
    let result = source.toReversed();
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], "Hello");
        else if (i === 5)
            shouldBe(result[i], "string");
    }
}
