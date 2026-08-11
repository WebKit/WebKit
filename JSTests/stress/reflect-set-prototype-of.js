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

shouldBe(Reflect.setPrototypeOf.length, 2);

shouldThrow(() => {
    Reflect.setPrototypeOf("hello");
}, `TypeError: Reflect.setPrototypeOf requires the first argument be an object`);

shouldThrow(() => {
    Reflect.setPrototypeOf(null);
}, `TypeError: Reflect.setPrototypeOf requires the first argument be an object`);

shouldThrow(() => {
    Reflect.setPrototypeOf({}, 30);
}, `TypeError: Reflect.setPrototypeOf requires the second argument be either an object or null`);

shouldThrow(() => {
    Reflect.setPrototypeOf({}, undefined);
}, `TypeError: Reflect.setPrototypeOf requires the second argument be either an object or null`);

var object = {};
var prototype = {};
shouldBe(Reflect.getPrototypeOf(object), Object.prototype);
shouldBe(Reflect.setPrototypeOf(object, prototype), true);
shouldBe(Reflect.getPrototypeOf(object), prototype);

var object = {};
shouldBe(Reflect.getPrototypeOf(object), Object.prototype);
shouldBe(Reflect.setPrototypeOf(object, null), true);
shouldBe(Reflect.getPrototypeOf(object), null);

var array = [];
var prototype = {};
shouldBe(Reflect.getPrototypeOf(array), Array.prototype);
shouldBe(Reflect.setPrototypeOf(array, prototype), true);
shouldBe(Reflect.getPrototypeOf(array), prototype);

var array = [];
shouldBe(Reflect.getPrototypeOf(array), Array.prototype);
shouldBe(Reflect.setPrototypeOf(array, null), true);
shouldBe(Reflect.getPrototypeOf(array), null);

var object = Object.create(null);
shouldBe(Reflect.getPrototypeOf(object), null);
shouldBe(Reflect.setPrototypeOf(object, Object.prototype), true);
shouldBe(Reflect.getPrototypeOf(object), Object.prototype);

// Extensible check.
var object = {};
shouldBe(Reflect.preventExtensions(object), true);
shouldBe(Reflect.setPrototypeOf(object, null), false);
shouldBe(Reflect.getPrototypeOf(object), Object.prototype);

// Cyclic check.
var prototype = {};
var object = { __proto__: prototype };
shouldBe(Reflect.setPrototypeOf(prototype, object), false);
shouldBe(Reflect.getPrototypeOf(prototype), Object.prototype);

