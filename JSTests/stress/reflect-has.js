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

shouldBe(Reflect.hasOwnProperty("has"), true);
shouldBe(Reflect.has.length, 2);

shouldThrow(() => {
    Reflect.has("hello", 42);
}, `TypeError: Reflect.has requires the first argument be an object`);

var object = { hello: 42 };
shouldBe(Reflect.has(object, 'hello'), true);
shouldBe(Reflect.has(object, 'world'), false);
shouldBe(Reflect.has(object, 'prototype'), false);
shouldBe(Reflect.has(object, '__proto__'), true);
shouldBe(Reflect.has(object, 'hasOwnProperty'), true);
shouldBe(Reflect.deleteProperty(object, 'hello'), true);
shouldBe(Reflect.has(object, 'hello'), false);

shouldBe(Reflect.has([], 'length'), true);
shouldBe(Reflect.has([0,1,2], 0), true);
shouldBe(Reflect.has([0,1,2], 200), false);

var object = {
    [Symbol.iterator]: 42
};
shouldBe(Reflect.has(object, Symbol.iterator), true);
shouldBe(Reflect.has(object, Symbol.unscopables), false);
shouldBe(Reflect.deleteProperty(object, Symbol.iterator), true);
shouldBe(Reflect.has(object, Symbol.iterator), false);

var toPropertyKey = {
    toString() {
        throw new Error('toString called.');
    }
};

shouldThrow(() => {
    Reflect.has("hello", toPropertyKey);
}, `TypeError: Reflect.has requires the first argument be an object`);

shouldThrow(() => {
    Reflect.has({}, toPropertyKey);
}, `Error: toString called.`);

var toPropertyKey = {
    toString() {
        return 'ok';
    }
};
shouldBe(Reflect.has({ 'ok': 42 }, toPropertyKey), true);
