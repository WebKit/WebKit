function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

Object.defineProperty(Array.prototype, 2, {
    get() {
        this.called = true;
        return 42;
    }
});

{
    let array = [];
    array.length = 42;
    shouldBe(array.lastIndexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = [20, 20];
    array.length = 42;
    shouldBe(array.lastIndexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = ["Hello"];
    array.length = 42;
    shouldBe(array.lastIndexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = [42.195];
    array.length = 42;
    shouldBe(array.lastIndexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = ["Hello"];
    array.length = 42;
    ensureArrayStorage(array);
    shouldBe(array.lastIndexOf(42), 2);
    shouldBe(array.called, true);
}
