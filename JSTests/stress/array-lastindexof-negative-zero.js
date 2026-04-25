function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let array = [42.195, -0.0];
    shouldBe(array.lastIndexOf(0), 1);
    shouldBe(array.lastIndexOf(-0), 1);
}
{
    let array = [42.195, 0, -0.0];
    shouldBe(array.lastIndexOf(0), 2);
    shouldBe(array.lastIndexOf(-0), 2);
}
{
    let array = [42, 0];
    shouldBe(array.lastIndexOf(0), 1);
    shouldBe(array.lastIndexOf(-0), 1);
}
{
    let array = [42, 0, 44];
    shouldBe(array.lastIndexOf(0), 1);
    shouldBe(array.lastIndexOf(-0), 1);
}
