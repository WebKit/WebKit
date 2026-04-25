var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
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
    var builtin = createBuiltin(`(function (obj) {
        return @getByIdDirect(obj, "hello");
    })`);
    noInline(builtin);

    var obj = { hello: 42, world:33 };
    for (var i = 0; i < 1e4; ++i)
        shouldBe(builtin(obj), 42);

    var obj2 = { hello: 22 };
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(builtin(obj), 42);
        shouldBe(builtin(obj2), 22);
    }

    var obj3 = { };
    for (var i = 0; i < 1e4; ++i)
        shouldBe(builtin(obj3), undefined);

    var obj4 = {
        __proto__: { hello: 33 }
    };
    for (var i = 0; i < 1e4; ++i)
        shouldBe(builtin(obj4), undefined);

    var target5 = "Hello";
    var target6 = 42;
    var target7 = false;
    var target8 = Symbol("Cocoa");
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(builtin(target5), undefined);
        shouldBe(builtin(target6), undefined);
        shouldBe(builtin(target7), undefined);
        shouldBe(builtin(target8), undefined);
    }

    shouldThrow(() => {
        builtin(null);
    }, `TypeError: null is not an object`);

    shouldThrow(() => {
        builtin(undefined);
    }, `TypeError: undefined is not an object`);

    shouldBe(builtin(obj), 42);
    shouldBe(builtin(obj2), 22);
    shouldBe(builtin(obj3), undefined);
    shouldBe(builtin(obj4), undefined);
    shouldBe(builtin(target5), undefined);
    shouldBe(builtin(target6), undefined);
    shouldBe(builtin(target7), undefined);
    shouldBe(builtin(target8), undefined);
}());

(function () {
    var builtin = createBuiltin(`(function (obj) {
        return @getByIdDirect(obj, "hello");
    })`);
    noInline(builtin);

    var obj = { };
    for (var i = 0; i < 1e4; ++i)
        shouldBe(builtin(obj), undefined);
    shouldBe(builtin(obj), undefined);
    obj.hello = 42;
    shouldBe(builtin(obj), 42);
}());


(function () {
    var builtin = createBuiltin(`(function (obj) {
        return @getByIdDirect(obj, "length");
    })`);
    noInline(builtin);

    var array = [0, 1, 2];
    for (var i = 0; i < 1e4; ++i)
        shouldBe(builtin(array), 3);
    shouldBe(builtin({}), undefined);

    var obj = { length:2 };
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(builtin(array), 3);
        shouldBe(builtin(obj), 2);
        shouldBe(builtin("Cocoa"), 5);
    }
}());
