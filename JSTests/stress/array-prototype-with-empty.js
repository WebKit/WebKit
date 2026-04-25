function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let source = new Array(0);
    let result = source.concat();
    shouldBe(result.length, 0);
}

{
    let source = new Array(2);
    source[0] = 42;
    source[1] = 43;
    let result = source.with(0, 44);
    shouldBe(result.length, 2);
    shouldBe(result[0], 44);
    shouldBe(result[1], 43);
}

{
    let source = new Array(2);
    source[0] = 42.195;
    source[1] = 43.195;
    let result = source.with(0, 44.195);
    shouldBe(result.length, 2);
    shouldBe(result[0], 44.195);
    shouldBe(result[1], 43.195);
}

{
    let source = new Array(6);
    let result = source.with(3, undefined);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = 42;
    let result = source.with(0, 43);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], 43);
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = 42.195;
    let result = source.with(0, 43.195);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], 43.195);
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = "string";
    let result = source.with(0, "stringstring");
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], "stringstring");
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = "string";
    $vm.ensureArrayStorage(source);
    source[1000] = "Hello";
    let result = source.with(0, "stringstring").with(1000, "HelloHello");
    shouldBe(result.length, 1001);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], "stringstring");
        else if (i === 1000)
            shouldBe(result[i], "HelloHello");
    }
}
