function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

{
    let source = {};
    Reflect.defineProperty(source, "hello", {
        enumerable: false,
        configurable: true,
        writable: true,
        value: 42,
    });
    let result = Object.assign({}, source);
    shouldBe(Reflect.getOwnPropertyDescriptor(result, "hello"), undefined);
}
{
    let source = {};
    Reflect.defineProperty(source, "hello", {
        enumerable: true,
        configurable: false,
        writable: true,
        value: 42,
    });
    let result = Object.assign({}, source);
    shouldBe(JSON.stringify(Reflect.getOwnPropertyDescriptor(result, "hello")), `{"value":42,"writable":true,"enumerable":true,"configurable":true}`);
}
{
    let source = {};
    Reflect.defineProperty(source, "hello", {
        enumerable: true,
        configurable: true,
        writable: false,
        value: 42,
    });
    let result = Object.assign({}, source);
    shouldBe(JSON.stringify(Reflect.getOwnPropertyDescriptor(result, "hello")), `{"value":42,"writable":true,"enumerable":true,"configurable":true}`);
}
{
    let source = {};
    Reflect.defineProperty(source, "hello", {
        enumerable: true,
        configurable: true,
        get() { return 42; },
        set() { }
    });
    let result = Object.assign({}, source);
    shouldBe(JSON.stringify(Reflect.getOwnPropertyDescriptor(result, "hello")), `{"value":42,"writable":true,"enumerable":true,"configurable":true}`);
}
