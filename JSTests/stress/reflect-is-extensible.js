function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

shouldBe(Reflect.isExtensible.length, 1);

shouldThrow(() => {
    Reflect.isExtensible("hello");
}, `TypeError: Reflect.isExtensible requires the first argument be an object`);

var object = { hello: 42 };
shouldBe(Reflect.isExtensible(object), true);
Object.preventExtensions(object);
shouldBe(Reflect.isExtensible(object), false);
