function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
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

shouldBe(String.raw.name, 'raw');
shouldBe(String.raw.length, 1);

shouldThrow(function () {
    String.raw();
}, "TypeError: String.raw requires template not be null or undefined");

shouldThrow(function () {
    String.raw(undefined);
}, "TypeError: String.raw requires template not be null or undefined");

shouldThrow(function () {
    String.raw({ raw: undefined });
}, "TypeError: String.raw requires template.raw not be null or undefined");

shouldThrow(function () {
    String.raw({ raw: null });
}, "TypeError: String.raw requires template.raw not be null or undefined");

shouldThrow(function () {
    String.raw({
        get length() {
            return new Error('template.length called');
        },

        raw: {
            get length() {
                throw new Error("template.raw.length called");
            }
        }
    });
}, "Error: template.raw.length called");

shouldBe(String.raw({
    raw: {
        length: -1
    }
}), "");

shouldBe(String.raw({
    raw: {
        length: -2.5
    }
}), "");

shouldBe(String.raw({
    raw: {
        length: -Infinity
    }
}), "");

shouldBe(String.raw({
    raw: {
        length: 0
    }
}), "");

shouldBe(String.raw({
    raw: {
        length: NaN
    }
}), "");

function generateTemplate() {
    var cooked = [];
    cooked.raw = Array.from(arguments);
    return cooked;
}

shouldBe(String.raw(generateTemplate("", ",", ",", ""), "Cocoa", "Cappuccino", "Matcha"), "Cocoa,Cappuccino,Matcha");
shouldBe(String.raw(generateTemplate("", ",", ",", ""), "Cocoa", "Cappuccino", "Matcha", "Hello"), "Cocoa,Cappuccino,Matcha");
shouldBe(String.raw(generateTemplate("", ",", ",", ""), "Cocoa", "Cappuccino"), "Cocoa,Cappuccino,");
shouldBe(String.raw(generateTemplate("", ",", ",", ""), "Cocoa"), "Cocoa,,");
shouldBe(String.raw(generateTemplate("", ",", ",", "")), ",,");

function Counter(p) {
    var count = 0;
    return {
        toString() {
            return count++;
        }
    };
}

var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter)), "0123");
var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter), counter), "01234");
var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter), counter, counter), "012345");
var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter), counter, counter, counter), "0123456");
var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter), counter, counter, counter, counter), "0123456");
var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter), counter, counter, counter, counter), "0123456");
var counter = Counter();
shouldBe(String.raw(generateTemplate(counter, counter, counter, counter), counter, counter, counter, counter, counter), "0123456");


shouldBe(String.raw({
    raw: {
        length: 3.5,
        0: "a",
        1: "b",
        2: "c"
    }
}, "d", "e", "f", "g"), "adbec");

shouldBe(String.raw({
    raw: {
        length: 2.3,
        0: "a",
        1: "b",
        2: "c"
    }
}, "d", "e", "f", "g"), "adb");

shouldBe(String.raw({
    raw: {
        length: 2.3,
        0: "a",
        2: "c"
    }
}, "d", "e", "f", "g"), "adundefined");

shouldBe(String.raw({
    raw: {
        length: 2.3,
        0: "a",
        1: "b",
        2: "c"
    }
}, undefined, "e", "f", "g"), "aundefinedb");
