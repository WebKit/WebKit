function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    return Object.keys(object);
}
noInline(test);

{
    let object = new String("Cocoa");
    for (let i = 0; i < 1e3; ++i) {
        let result = test(object);
        shouldBe(result.length, 5);
        shouldBe(result[0], '0');
        shouldBe(result[1], '1');
        shouldBe(result[2], '2');
        shouldBe(result[3], '3');
        shouldBe(result[4], '4');
    }

    object.Cocoa = 42;
    let result = test(object);
    shouldBe(result.length, 6);
    shouldBe(result[0], '0');
    shouldBe(result[1], '1');
    shouldBe(result[2], '2');
    shouldBe(result[3], '3');
    shouldBe(result[4], '4');
    shouldBe(result[5], 'Cocoa');
}

{
    let object = new String("Cocoa");
    for (let i = 0; i < 1e3; ++i) {
        let result = test(object);
        shouldBe(result.length, 5);
        shouldBe(result[0], '0');
        shouldBe(result[1], '1');
        shouldBe(result[2], '2');
        shouldBe(result[3], '3');
        shouldBe(result[4], '4');
    }

    object[8] = 42;
    let result = test(object);
    shouldBe(result.length, 6);
    shouldBe(result[0], '0');
    shouldBe(result[1], '1');
    shouldBe(result[2], '2');
    shouldBe(result[3], '3');
    shouldBe(result[4], '4');
    shouldBe(result[5], '8');
}
