function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedArray extends Array {
    get 2() {
        this.called = true;
        return 42;
    }
}

{
    let array = [];
    array.__proto__ = DerivedArray.prototype;
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = [20, 20];
    array.__proto__ = DerivedArray.prototype;
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = ["Hello"];
    array.__proto__ = DerivedArray.prototype;
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = [42.195];
    array.__proto__ = DerivedArray.prototype;
    array.length = 42;
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
{
    let array = ["Hello"];
    array.__proto__ = DerivedArray.prototype;
    array.length = 42;
    ensureArrayStorage(array);
    shouldBe(array.indexOf(42), 2);
    shouldBe(array.called, true);
}
