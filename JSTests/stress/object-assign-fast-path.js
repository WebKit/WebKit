var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
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

function checkProperty(object, name, value, attributes = { writable: true, enumerable: true, configurable: true })
{
    var desc = Object.getOwnPropertyDescriptor(object, name);
    shouldBe(!!desc, true);
    shouldBe(desc.writable, attributes.writable);
    shouldBe(desc.enumerable, attributes.enumerable);
    shouldBe(desc.configurable, attributes.configurable);
    shouldBe(desc.value, value);
}

{
    let result = Object.assign({}, RegExp);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["$1","$2","$3","$4","$5","$6","$7","$8","$9","input","lastMatch","lastParen","leftContext","multiline","rightContext"]`);
}
{
    function Hello() { }
    let result = Object.assign(Hello, {
        ok: 42
    });

    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["arguments","caller","length","name","ok","prototype"]`);
    checkProperty(result, "ok", 42);
}
{
    let result = Object.assign({ ok: 42 }, { 0: 0, 1: 1 });
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["0","1","ok"]`);
    checkProperty(result, "ok", 42);
    checkProperty(result, "0", 0);
    checkProperty(result, "1", 1);
}
{
    let object = { 0: 0, 1: 1 };
    ensureArrayStorage(object);
    let result = Object.assign({ ok: 42 }, object);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["0","1","ok"]`);
    checkProperty(result, "ok", 42);
    checkProperty(result, "0", 0);
    checkProperty(result, "1", 1);
}
{
    let called = false;
    let result = Object.assign({}, {
        get hello() {
            called = true;
            return 42;
        }
    });
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["hello"]`);
    shouldBe(called, true);
    checkProperty(result, "hello", 42);
}
{
    let object = {};
    Object.defineProperty(object, "__proto__", {
        value: 42,
        enumerable: true,
        writable: true,
        configurable: true
    });
    checkProperty(object, "__proto__", 42);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(object).sort()), `["__proto__"]`);
    let result = Object.assign({}, object);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `[]`);
    shouldBe(Object.getOwnPropertyDescriptor(result, "__proto__"), undefined);
    shouldBe(result.__proto__, Object.prototype);
}
{
    let object = {};
    Object.defineProperty(object, "hello", {
        value: 42,
        writable: false,
        enumerable: true,
        configurable: false
    });
    checkProperty(object, "hello", 42, { writable: false, enumerable: true, configurable: false });
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(object).sort()), `["hello"]`);
    shouldThrow(() => {
        Object.assign(object, { hello: 50 });
    }, `TypeError: Attempted to assign to readonly property.`);
}
{
    let counter = 0;
    let helloCalled = null;
    let okCalled = null;
    let source = {};
    source.hello = 42;
    source.ok = 52;
    checkProperty(source, "hello", 42);
    checkProperty(source, "ok", 52);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(source)), `["hello","ok"]`);

    let result = Object.assign({
        set hello(value) {
            this.__hello = value;
            helloCalled = counter++;
        },
        set ok(value) {
            this.__ok = value;
            okCalled = counter++;
        }
    }, source);
    checkProperty(result, "__hello", 42);
    checkProperty(result, "__ok", 52);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["__hello","__ok","hello","ok"]`);
    shouldBe(helloCalled, 0);
    shouldBe(okCalled, 1);
}
{
    let builtin = createBuiltin(`(function (obj) {
        return @getByIdDirectPrivate(obj, "generatorState");
    })`);
    function* hello() { }
    let generator = hello();
    shouldBe(typeof builtin(generator), "number");
    let result = Object.assign({}, generator);
    shouldBe(typeof builtin(result), "undefined");
}
{
    let object = {};
    let setterCalledWithValue = null;
    let result = Object.assign(object, {
        get hello() {
            Object.defineProperty(object, "added", {
                get() {
                    return 42;
                },
                set(value) {
                    setterCalledWithValue = value;
                }
            });
            return 0;
        }
    }, {
        added: "world"
    });
    shouldBe(result.added, 42);
    shouldBe(result.hello, 0);
    shouldBe(setterCalledWithValue, "world");
}
