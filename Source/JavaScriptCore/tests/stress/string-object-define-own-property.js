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

var string = new String("Cocoa");
shouldBe(Reflect.defineProperty(string, 0, {
}), true);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    configurable: false
}), true);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    configurable: true
}), false);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    enumerable: true
}), true);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    enumerable: false
}), false);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    writable: false,
}), true);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    writable: false,
    value: 'C',
    configurable: true
}), false);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    writable: true,
    value: 52,
}), false);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    value: 52,
}), false);
shouldBe(Reflect.get(string, 0), 'C');

shouldBe(Reflect.defineProperty(string, 0, {
    writable: false,
    value: 'C',
    configurable: false
}), true);
shouldBe(Reflect.get(string, 0), 'C');

// Out of range.
shouldBe(Reflect.defineProperty(string, 2000, {
    value: 'Cappuccino'
}), true);
shouldBe(Reflect.get(string, 2000), 'Cappuccino');
shouldBe(Reflect.defineProperty(string, "Hello", {
    value: 'Cappuccino'
}), true);
shouldBe(Reflect.get(string, "Hello"), 'Cappuccino');
