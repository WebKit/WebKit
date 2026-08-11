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

shouldBe(Reflect.apply.length, 3);

shouldThrow(() => {
    Reflect.apply("hello", 42);
}, `TypeError: Reflect.apply requires the first argument be a function`);

shouldThrow(() => {
    Reflect.apply(function () { }, 42, null);
}, `TypeError: Reflect.apply requires the third argument be an object`);

shouldThrow(() => {
    var array = {
        get length() {
            throw new Error("ok");
        },
        get 0() {
            throw new Error("ng");
        }
    };
    Reflect.apply(function () { }, {}, array);
}, `Error: ok`);

shouldThrow(() => {
    var array = {
        get length() {
            return 1;
        },
        get 0() {
            throw new Error("ok");
        }
    };
    Reflect.apply(function () { }, {}, array);
}, `Error: ok`);

var array = {
    get length() {
        return 0;
    },
    get 0() {
        throw new Error("ng");
    }
};
shouldBe(Reflect.apply(function () { return arguments.length }, {}, array), 0);

var globalObject = this;
shouldBe(Reflect.apply(function () {
    "use strict";
    shouldBe(arguments[0], 0);
    shouldBe(arguments[1], 1);
    shouldBe(arguments[2], 2);
    shouldBe(this, null);
    return arguments.length;
}, null, [0,1,2]), 3)

shouldBe(Reflect.apply(function () {
    shouldBe(arguments[0], 0);
    shouldBe(arguments[1], 1);
    shouldBe(arguments[2], 2);
    shouldBe(this, globalObject);
    return arguments.length;
}, null, [0,1,2]), 3)

var thisObject = {};
shouldBe(Reflect.apply(function () {
    "use strict";
    shouldBe(this, thisObject);
    return arguments.length;
}, thisObject, []), 0)

shouldBe(Reflect.apply(function () {
    shouldBe(this, thisObject);
    return arguments.length;
}, thisObject, []), 0)
