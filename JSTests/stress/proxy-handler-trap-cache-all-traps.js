function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}`);
}

function exercise(proxy) {
    delete proxy.x;
    Object.preventExtensions(proxy);
    Object.isExtensible(proxy);
    Object.setPrototypeOf(proxy, Object.getPrototypeOf(proxy));
    Object.getPrototypeOf(proxy);
}

function testTrapsAddedAndRemoved(handler) {
    let log = [];
    let target = function () { };
    let proxy = new Proxy(target, handler);

    for (let i = 0; i < testLoopCount; ++i) {
        exercise(proxy);
        new proxy();
    }
    shouldBe(Object.isExtensible(target), false);

    handler.deleteProperty = function (t, key) { log.push("deleteProperty"); return Reflect.deleteProperty(t, key); };
    handler.preventExtensions = function (t) { log.push("preventExtensions"); return Reflect.preventExtensions(t); };
    handler.isExtensible = function (t) { log.push("isExtensible"); return Reflect.isExtensible(t); };
    handler.setPrototypeOf = function (t, p) { log.push("setPrototypeOf"); return Reflect.setPrototypeOf(t, p); };
    handler.getPrototypeOf = function (t) { log.push("getPrototypeOf"); return Reflect.getPrototypeOf(t); };
    handler.construct = function () { log.push("construct"); return { constructed: true }; };

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        exercise(proxy);
        shouldBe(new proxy().constructed, true);
        shouldBe(log.join(), "deleteProperty,preventExtensions,isExtensible,getPrototypeOf,setPrototypeOf,getPrototypeOf,construct");
    }

    for (let name of ["deleteProperty", "preventExtensions", "isExtensible", "setPrototypeOf", "getPrototypeOf", "construct"])
        delete handler[name];

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        exercise(proxy);
        shouldBe(typeof new proxy(), "object");
        shouldBe(log.length, 0);
    }

    handler.getPrototypeOf = 42;
    shouldThrow(() => Object.getPrototypeOf(proxy), TypeError);
    handler.construct = 42;
    shouldThrow(() => new proxy(), TypeError);
}

testTrapsAddedAndRemoved({ });

// Traps stored at property offsets too large for the one-byte trap cache entry, on a handler that is not a dictionary.
{
    let source = "return { ";
    for (let i = 0; i < 300; ++i)
        source += "p" + i + ": " + i + ", ";
    source += "};";
    let handler = new Function(source)();
    let log = [];
    handler.deleteProperty = function (t, key) { log.push("deleteProperty"); return Reflect.deleteProperty(t, key); };
    handler.preventExtensions = function (t) { log.push("preventExtensions"); return Reflect.preventExtensions(t); };
    handler.isExtensible = function (t) { log.push("isExtensible"); return Reflect.isExtensible(t); };
    handler.setPrototypeOf = function (t, p) { log.push("setPrototypeOf"); return Reflect.setPrototypeOf(t, p); };
    handler.getPrototypeOf = function (t) { log.push("getPrototypeOf"); return Reflect.getPrototypeOf(t); };
    handler.construct = function () { log.push("construct"); return { constructed: true }; };

    let proxy = new Proxy(function () { }, handler);
    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        exercise(proxy);
        shouldBe(new proxy().constructed, true);
        shouldBe(log.join(), "deleteProperty,preventExtensions,isExtensible,getPrototypeOf,setPrototypeOf,getPrototypeOf,construct");
    }
}

{
    let proto = { };
    let handler = Object.create(proto);
    let proxy = new Proxy({ }, handler);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(Object.isExtensible(proxy), true);
    let called = false;
    proto.isExtensible = function (t) { called = true; return Reflect.isExtensible(t); };
    shouldBe(Object.isExtensible(proxy), true);
    shouldBe(called, true);
}
