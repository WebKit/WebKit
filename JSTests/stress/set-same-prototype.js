function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var proto = {};
var object = Object.preventExtensions(Object.create(proto));

shouldBe(Object.setPrototypeOf(object, proto), object);
shouldThrow(() => {
    Object.setPrototypeOf(object, {});
}, `TypeError: Attempted to assign to readonly property.`);
shouldBe(Reflect.getPrototypeOf(object), proto);

shouldBe(Reflect.setPrototypeOf(object, proto), true);
shouldBe(Reflect.setPrototypeOf(object, {}), false);
shouldBe(Reflect.getPrototypeOf(object), proto);

object.__proto__ = proto;
shouldThrow(() => {
    object.__proto__ = {};
}, `TypeError: Attempted to assign to readonly property.`);
shouldBe(object.__proto__, proto);
