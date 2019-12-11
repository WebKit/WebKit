function createBuffer() {
    return [NaN, 2.0585345];
}
noInline(createBuffer);

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(a + " should be === to " + b);
}

function test() {
    let array = createBuffer();
    array[1000000] = "test";
    shouldBe(createBuffer()[1000000], undefined);
    array = createBuffer();
    let o = Object.create(array);
    o[1000000] = "test";
    shouldBe(array[1000000], undefined);
    shouldBe(createBuffer()[1000000], undefined);
    shouldBe(Object.create(createBuffer())[1000000], undefined);
}
noInline(test);

for (let i = 0; i < 10000; i++)
    test();
