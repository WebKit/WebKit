function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


{
    let array = [42, , , 0];
    shouldBe(array.indexOf(Number.NaN), -1);
    shouldBe(array.indexOf(0), 3);
}
{
    let array = [42.195, , , 0];
    shouldBe(array.indexOf(Number.NaN), -1);
}
{
    let array = [42.195, Number.NaN, , 0];
    shouldBe(array.indexOf(Number.NaN), -1);
}
