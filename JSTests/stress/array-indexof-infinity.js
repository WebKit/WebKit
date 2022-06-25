function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let array = [42.195, -0.0, -Infinity];
    shouldBe(array.indexOf(Infinity), -1);
    shouldBe(array.indexOf(-Infinity), 2);
    shouldBe(array.indexOf(42), -1);
}
{
    let array = [1, 2, 3, 0];
    shouldBe(array.indexOf(Infinity), -1);
    shouldBe(array.indexOf(-Infinity), -1);
}
{
    let array = ["String", 42.5, Infinity, 33];
    shouldBe(array.indexOf(Infinity), 2);
    shouldBe(array.indexOf(-Infinity), -1);
}
