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
    array[100000000] = "test";
    shouldBe(createBuffer()[100000000], undefined);
    array = createBuffer();
    let o = Object.create(array);
    o[100000000] = "test";
    shouldBe(array[100000000], undefined);
    shouldBe(createBuffer()[100000000], undefined);
    shouldBe(Object.create(createBuffer())[100000000], undefined);
}
noInline(test);

for (let i = 0; i < 10000; i++)
    test();
