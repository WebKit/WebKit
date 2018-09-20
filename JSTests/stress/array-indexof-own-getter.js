function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let array = [];
    Object.defineProperty(array, 2, {
        get() {
            this.called = true;
            return 42;
        }
    });
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = [20, 20];
    Object.defineProperty(array, 2, {
        get() {
            this.called = true;
            return 42;
        }
    });
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = ["Hello"];
    Object.defineProperty(array, 2, {
        get() {
            this.called = true;
            return 42;
        }
    });
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = [42.195];
    Object.defineProperty(array, 2, {
        get() {
            this.called = true;
            return 42;
        }
    });
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = ["Hello"];
    Object.defineProperty(array, 2, {
        get() {
            this.called = true;
            return 42;
        }
    });
    array.length = 42;
    ensureArrayStorage(array);
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
