var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    var builtin = createBuiltin(`(function (obj, value) {
        return @putByIdDirect(obj, "hello", value);
    })`);
    noInline(builtin);

    var setValue = null;
    var object = {
        __proto__: {
            hello: 30
        }
    };
    builtin(object, 42);
    shouldBe(object.hello, 42);
    shouldBe(object.hasOwnProperty("hello"), true);
}());
