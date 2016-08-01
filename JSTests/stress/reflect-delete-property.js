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

shouldBe(Reflect.deleteProperty.length, 2);

shouldThrow(() => {
    Reflect.deleteProperty("hello", 42);
}, `TypeError: Reflect.deleteProperty requires the first argument be an object`);

var object = { hello: 42 };
shouldBe(object.hello, 42);
shouldBe(object.hasOwnProperty('hello'), true);
shouldBe(Reflect.deleteProperty(object, 'hello'), true);
shouldBe(object.hasOwnProperty('hello'), false);
shouldBe(Reflect.deleteProperty(object, 'hasOwnProperty'), true);
shouldBe(object.hasOwnProperty('hasOwnProperty'), false);

shouldBe(Reflect.deleteProperty([], 'length'), false);
shouldBe(Reflect.deleteProperty([0,1,2], 0), true);

var object = {
    [Symbol.iterator]: 42
};
shouldBe(object.hasOwnProperty(Symbol.iterator), true);
shouldBe(object[Symbol.iterator], 42);
shouldBe(Reflect.deleteProperty(object, Symbol.iterator), true);
shouldBe(object.hasOwnProperty(Symbol.iterator), false);

var toPropertyKey = {
    toString() {
        throw new Error('toString called.');
    }
};

shouldThrow(() => {
    Reflect.deleteProperty("hello", toPropertyKey);
}, `TypeError: Reflect.deleteProperty requires the first argument be an object`);

shouldThrow(() => {
    Reflect.deleteProperty({}, toPropertyKey);
}, `Error: toString called.`);
