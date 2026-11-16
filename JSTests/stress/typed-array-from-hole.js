function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let a = Int32Array.from([0, , , 6]);
    shouldBe(a.length, 4);
    shouldBe(a[0], 0);
    shouldBe(a[1], 0);
    shouldBe(a[2], 0);
    shouldBe(a[3], 6);
}
{
    let a = Int32Array.from([0.2, , , 6.1]);
    shouldBe(a.length, 4);
    shouldBe(a[0], 0);
    shouldBe(a[1], 0);
    shouldBe(a[2], 0);
    shouldBe(a[3], 6);
}
{
    let a = Float64Array.from([0, , , 6]);
    shouldBe(a.length, 4);
    shouldBe(a[0], 0);
    shouldBe(Number.isNaN(a[1]), true);
    shouldBe(Number.isNaN(a[2]), true);
    shouldBe(a[3], 6);
}
{
    let a = Float64Array.from([0.2, , , 6.1]);
    shouldBe(a.length, 4);
    shouldBe(a[0], 0.2);
    shouldBe(Number.isNaN(a[1]), true);
    shouldBe(Number.isNaN(a[2]), true);
    shouldBe(a[3], 6.1);
}
