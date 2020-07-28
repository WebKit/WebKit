//@ skip if ["arm", "mips"].include?($architecture)

function shouldBe(actual, expected)
{
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

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(target({}), Object.prototype);
        shouldBe(target((function() {}).prototype), Object.prototype);
        shouldBe(target((class {}).prototype), Object.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    var F = function() {};
    var C = class {};

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(target(new F()), F.prototype);
        shouldBe(target(new C()), C.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(target(Array.prototype), Object.prototype);
        shouldBe(target([]), Array.prototype);
        shouldBe(target(Array(3)), Array.prototype);
        shouldBe(target(new Array(1, 2, 3)), Array.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(target(Object.prototype), null);
        shouldBe(target(Object.create(null)), null);
        shouldBe(target(Object.setPrototypeOf({}, null)), null);
        shouldBe(target({__proto__: null}), null);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(target(function() {}), Function.prototype);
        shouldBe(target(class {}), Function.prototype);
        shouldBe(target(() => {}), Function.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    var regExp = /(?:)/;
    var date = new Date();
    var map = new Map();
    var weakSet = new WeakSet();
    var promise = Promise.resolve(1);

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(target(regExp), RegExp.prototype);
        shouldBe(target(date), Date.prototype);
        shouldBe(target(map), Map.prototype);
        shouldBe(target(weakSet), WeakSet.prototype);
        shouldBe(target(promise), Promise.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    function makePolyProtoObject() {
        function foo() {
            class C {
                constructor() {
                    this._field = 42;
                }
            }
            lastProto = C.prototype;
            return new C();
        }
        for (let i = 0; i < 1000; ++i)
            foo();
        return foo();
    }

    var polyProtoObject = makePolyProtoObject();

    for (var i = 0; i < 1e5; ++i)
        shouldBe(target(polyProtoObject), polyProtoObject.constructor.prototype);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    var proxyPrototype = {};
    var proxy = new Proxy({}, {
        getPrototypeOf: () => proxyPrototype,
    });

    for (var i = 0; i < 1e5; ++i)
        shouldBe(target(proxy), proxyPrototype);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i) {
        shouldThrow(() => target(null), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(undefined), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i)
        shouldThrow(() => target("Cocoa"), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i)
        shouldThrow(() => target(42), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i)
        shouldThrow(() => target(42.195), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i)
        shouldThrow(() => target(true), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i)
        shouldThrow(() => target(Symbol()), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e5; ++i) {
        shouldThrow(() => target("Cocoa"), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(42), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(42.195), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(true), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(Symbol()), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(1n), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
    }
}());
