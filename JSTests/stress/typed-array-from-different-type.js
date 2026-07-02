function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let a = new Int32Array([0, 1, 2]);
    let b = Float64Array.from(a);
    shouldBe(b.length, 3);
    shouldBe(b[0], 0);
    shouldBe(b[1], 1);
    shouldBe(b[2], 2);
}
{
    let a = new Float32Array([0, 1, 2]);
    let b = Int32Array.from(a);
    shouldBe(b.length, 3);
    shouldBe(b[0], 0);
    shouldBe(b[1], 1);
    shouldBe(b[2], 2);
}
