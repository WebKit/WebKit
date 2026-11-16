function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let source = new Array(0);
    let result = Array.from(source);
    shouldBe(result.length, 0);
}

{
    let source = new Array(2);
    source[0] = 42;
    source[1] = 43;
    let result = Array.from(source);
    shouldBe(result.length, 2);
    shouldBe(result[0], 42);
    shouldBe(result[1], 43);
}

{
    let source = new Array(2);
    source[0] = 42.195;
    source[1] = 43.195;
    let result = Array.from(source);
    shouldBe(result.length, 2);
    shouldBe(result[0], 42.195);
    shouldBe(result[1], 43.195);
}

{
    let source = new Array(6);
    let result = Array.from(source);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = 42;
    let result = Array.from(source);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], 42);
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = 42.195;
    let result = Array.from(source);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], 42.195);
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = "string";
    let result = Array.from(source);
    shouldBe(result.length, 6);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], "string");
        else
            shouldBe(result[i], undefined);
    }
}

{
    let source = new Array(6);
    source[0] = "string";
    $vm.ensureArrayStorage(source);
    source[1000] = "Hello";
    let result = Array.from(source);
    shouldBe(result.length, 1001);
    for (var i = 0; i < result.length; ++i) {
        shouldBe(result.hasOwnProperty(i), true);
        if (i === 0)
            shouldBe(result[i], "string");
        else if (i === 1000)
            shouldBe(result[i], "Hello");
        else
            shouldBe(result[i], undefined);
    }
}
