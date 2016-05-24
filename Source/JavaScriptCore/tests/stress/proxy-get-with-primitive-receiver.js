function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


shouldBe(Reflect.get(new Proxy({}, {}), 0, 1), undefined);
shouldBe(Reflect.get(new Proxy({}, {}), 'hello', 1), undefined);

{
    let target = {};
    let handlers = {
        get: function(theTarget, propName, receiver) {
            // Receiver can be a primitive value.
            shouldBe(receiver, 1);
            return 42;
        }
    };
    let proxy = new Proxy(target, handlers);
    shouldBe(Reflect.get(proxy, 0, 1), 42);
    shouldBe(Reflect.get(proxy, 'hello', 1), 42);
}
