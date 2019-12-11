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

shouldBe(Reflect.defineProperty.length, 3);

shouldThrow(() => {
    Reflect.defineProperty("hello");
}, `TypeError: Reflect.defineProperty requires the first argument be an object`);

shouldThrow(() => {
    Reflect.defineProperty(null);
}, `TypeError: Reflect.defineProperty requires the first argument be an object`);

var object = {};
shouldBe(object[42], undefined);
shouldBe(object.hello, undefined);
shouldBe(Reflect.defineProperty(object, 42, {
    value: 42
}), true);
shouldBe(Reflect.defineProperty(object, 'hello', {
    value: 50
}), true);
shouldBe(object[42], 42);
shouldBe(object.hello, 50);

function testDescriptor(expected, actual) {
    shouldBe(expected.enumerable, actual.enumerable);
    shouldBe(expected.configurable, actual.configurable);
    shouldBe(expected.writable, actual.writable);
    shouldBe(expected.value, actual.value);
    shouldBe(expected.get, actual.get);
    shouldBe(expected.set, actual.set);
}

function getter() { }
function setter() { }

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: false,
    writable: false,
    enumerable: false,
    value: undefined
});


var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    writable: false,
    enumerable: false,
    value: undefined
});

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    writable: false,
    enumerable: true,
    value: undefined
});

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true,
    writable: true
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    writable: true,
    enumerable: true,
    value: undefined
});

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true,
    writable: true,
    value: 42
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    writable: true,
    enumerable: true,
    value: 42
});

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true,
    get: getter
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    enumerable: true,
    get: getter,
    set: undefined
});

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true,
    set: setter
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    enumerable: true,
    get: undefined,
    set: setter
});

var object = {};
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true,
    set: setter,
    get: getter
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    enumerable: true,
    get: getter,
    set: setter
});

shouldThrow(() => {
    var object = {};
    Reflect.defineProperty(object, 'cocoa', {
        configurable: true,
        enumerable: true,
        set: setter,
        get: getter,
        value: 42
    });
}, `TypeError: Invalid property.  'value' present on property with getter or setter.`);

shouldThrow(() => {
    var object = {};
    Reflect.defineProperty(object, 'cocoa', {
        configurable: true,
        enumerable: true,
        value: 42,
        set: setter,
        get: getter
    });
}, `TypeError: Invalid property.  'value' present on property with getter or setter.`);

shouldThrow(() => {
    var object = {};
    Reflect.defineProperty(object, 'cocoa', {
        configurable: true,
        enumerable: true,
        writable: false,
        get: getter
    });
}, `TypeError: Invalid property.  'writable' present on property with getter or setter.`);

var object = { cocoa: 42 };
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    configurable: true,
    enumerable: true,
    writable: false,
    value: 50
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    enumerable: true,
    writable: false,
    value: 50
});


var object = { cocoa: 42 };
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    writable: false,
    value: 50
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    enumerable: true,
    writable: false,
    value: 50
});
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    writable: true,
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: true,
    enumerable: true,
    writable: true,
    value: 50
});
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    writable: false,
    configurable: false,
    value: 50
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: false,
    enumerable: true,
    writable: false,
    value: 50
});
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    writable: true,
}), false);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: false,
    enumerable: true,
    writable: false,
    value: 50
});
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    enumerable: false,
}), false);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: false,
    enumerable: true,
    writable: false,
    value: 50
});
shouldBe(Reflect.defineProperty(object, 'cocoa', {
    enumerable: true,
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(object, 'cocoa'), {
    configurable: false,
    enumerable: true,
    writable: false,
    value: 50
});

var array = [];
shouldBe(Reflect.defineProperty(array, 'length', {
    get: getter,
    set: setter
}), false);
testDescriptor(Object.getOwnPropertyDescriptor(array, 'length'), {
    configurable: false,
    enumerable: false,
    writable: true,
    value: 0
});
shouldBe(Reflect.defineProperty(array, 'length', {
    writable: false,
    value: 30
}), true);
testDescriptor(Object.getOwnPropertyDescriptor(array, 'length'), {
    configurable: false,
    enumerable: false,
    writable: false,
    value: 30
});
array.length = 40;
testDescriptor(Object.getOwnPropertyDescriptor(array, 'length'), {
    configurable: false,
    enumerable: false,
    writable: false,
    value: 30
});
