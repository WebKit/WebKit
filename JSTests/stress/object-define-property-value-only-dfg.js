function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function shouldBeTrue(actual, message) {
    shouldBe(actual, true, message);
}

function shouldBeFalse(actual, message) {
    shouldBe(actual, false, message);
}

function checkDescriptor(obj, key, expectedValue, expectedWritable, expectedEnumerable, expectedConfigurable) {
    const desc = Object.getOwnPropertyDescriptor(obj, key);
    shouldBe(desc.value, expectedValue, "value");
    shouldBe(desc.writable, expectedWritable, "writable");
    shouldBe(desc.enumerable, expectedEnumerable, "enumerable");
    shouldBe(desc.configurable, expectedConfigurable, "configurable");
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", { value: 42 });
        shouldBe(obj.prop, 42);
        checkDescriptor(obj, "prop", 42, false, false, false);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        let obj = {};
        Object.defineProperty(obj, "num", { value: 123 });
        shouldBe(obj.num, 123);

        obj = {};
        Object.defineProperty(obj, "str", { value: "hello" });
        shouldBe(obj.str, "hello");

        obj = {};
        Object.defineProperty(obj, "bool", { value: true });
        shouldBe(obj.bool, true);

        obj = {};
        Object.defineProperty(obj, "undef", { value: undefined });
        shouldBe(obj.undef, undefined);
        shouldBeTrue("undef" in obj);

        obj = {};
        Object.defineProperty(obj, "nul", { value: null });
        shouldBe(obj.nul, null);

        const innerObj = { inner: true };
        obj = {};
        Object.defineProperty(obj, "obj", { value: innerObj });
        shouldBe(obj.obj, innerObj);
        shouldBe(obj.obj.inner, true);

        const fn = function() { return 99; };
        obj = {};
        Object.defineProperty(obj, "fn", { value: fn });
        shouldBe(obj.fn, fn);
        shouldBe(obj.fn(), 99);
    }
}

{
    const sym = Symbol("test");
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, sym, { value: "symbol value" });
        shouldBe(obj[sym], "symbol value");
        checkDescriptor(obj, sym, "symbol value", false, false, false);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", { value: 42, writable: true });
        shouldBe(obj.prop, 42);
        checkDescriptor(obj, "prop", 42, true, false, false);
        obj.prop = 100;
        shouldBe(obj.prop, 100);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", { value: 42, enumerable: true });
        shouldBe(obj.prop, 42);
        checkDescriptor(obj, "prop", 42, false, true, false);
        const keys = Object.keys(obj);
        shouldBe(keys.length, 1);
        shouldBe(keys[0], "prop");
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", { value: 42, configurable: true });
        shouldBe(obj.prop, 42);
        checkDescriptor(obj, "prop", 42, false, false, true);
        delete obj.prop;
        shouldBeFalse("prop" in obj);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        let storage = 0;
        const obj = {};
        Object.defineProperty(obj, "prop", {
            get: function() { return storage; },
            set: function(v) { storage = v; }
        });
        shouldBe(obj.prop, 0);
        obj.prop = 42;
        shouldBe(obj.prop, 42);
        shouldBe(storage, 42);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", {
            get: function() { return 99; }
        });
        shouldBe(obj.prop, 99);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        let captured = null;
        const obj = {};
        Object.defineProperty(obj, "prop", {
            set: function(v) { captured = v; }
        });
        obj.prop = "test";
        shouldBe(captured, "test");
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", { value: 1, configurable: true });
        shouldBe(obj.prop, 1);
        Object.defineProperty(obj, "prop", { value: 2 });
        shouldBe(obj.prop, 2);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "prop", { value: 42, configurable: false });
        let threw = false;
        try {
            Object.defineProperty(obj, "prop", { value: 100 });
        } catch (e) {
            threw = true;
        }
        shouldBeTrue(threw);
        shouldBe(obj.prop, 42);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        const desc = { value: i };
        Object.defineProperty(obj, "prop", desc);
        shouldBe(obj.prop, i);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const obj = {};
        Object.defineProperty(obj, "a", { value: 1 });
        Object.defineProperty(obj, "b", { value: 2 });
        Object.defineProperty(obj, "c", { value: 3 });
        shouldBe(obj.a, 1);
        shouldBe(obj.b, 2);
        shouldBe(obj.c, 3);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        function Ctor() {}
        Object.defineProperty(Ctor.prototype, "prop", { value: 42 });
        const instance = new Ctor();
        shouldBe(instance.prop, 42);
    }
}

{
    for (let i = 0; i < testLoopCount; i++) {
        const exports = {};
        Object.defineProperty(exports, "__esModule", { value: true });
        shouldBe(exports.__esModule, true);
        checkDescriptor(exports, "__esModule", true, false, false, false);
    }
}
