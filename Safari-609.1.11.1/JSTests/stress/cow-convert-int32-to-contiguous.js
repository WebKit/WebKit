function createBuffer() {
    return [1, 2];
}
noInline(createBuffer);

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(a + " should be === to " + b);
}

function test() {
    let array = createBuffer();
    array[-1] = "test";
    shouldBe(createBuffer()[-1], undefined);
    array = createBuffer();
    array[1] = "test";
    shouldBe(createBuffer()[1], 2);
    array = createBuffer();
    let o = Object.create(array);
    o[1] = "test";
    shouldBe(array[1], 2);
    shouldBe(createBuffer()[1], 2);
    shouldBe(Object.create(createBuffer())[1], 2);
}
noInline(test);

for (let i = 0; i < 10000; i++)
    test();
