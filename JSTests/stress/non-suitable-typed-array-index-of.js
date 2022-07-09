function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let array = new Uint8Array([0, 1, 2, 3, 0xff]);
    shouldBe(array.indexOf(-1), -1);
    shouldBe(array.indexOf(0xff), 4);
    shouldBe(array.indexOf(0xffff), -1);
    shouldBe(array.includes(-1), false);
    shouldBe(array.includes(0xff), true);
    shouldBe(array.includes(0xffff), false);
}
{
    let array = new Uint8ClampedArray([0, 1, 2, 3, 0xff]);
    shouldBe(array.indexOf(-1), -1);
    shouldBe(array.indexOf(0xff), 4);
    shouldBe(array.indexOf(0xffff), -1);
    shouldBe(array.includes(-1), false);
    shouldBe(array.includes(0xff), true);
    shouldBe(array.includes(0xffff), false);
}
{
    let array = new Int8Array([0, 1, 2, 3, -1]);
    shouldBe(array.indexOf(-1), 4);
    shouldBe(array.indexOf(0xff), -1);
    shouldBe(array.indexOf(0xffff), -1);
    shouldBe(array.includes(-1), true);
    shouldBe(array.includes(0xff), false);
    shouldBe(array.includes(0xffff), false);
}
